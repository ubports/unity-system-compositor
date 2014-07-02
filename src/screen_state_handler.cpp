/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "screen_state_handler.h"

#include <mir/main_loop.h>
#include <mir/time/timer.h>
#include <mir/compositor/compositor.h>
#include <mir/default_server_configuration.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>

#include <cstdio>
#include "dbus_screen.h"
#include "dbus_screen_observer.h"
#include "powerd_mediator.h"
#include "power_state_change_reason.h"

namespace mi = mir::input;
namespace mc = mir::compositor;
namespace mg = mir::graphics;

ScreenStateHandler::ScreenStateHandler(std::shared_ptr<mir::DefaultServerConfiguration> const& config,
                                       std::chrono::milliseconds power_off_timeout,
                                       std::chrono::milliseconds dimmer_timeout)
    : current_power_mode{MirPowerMode::mir_power_mode_on},
      restart_timers{true},
      power_off_timeout{power_off_timeout},
      dimming_timeout{dimmer_timeout},
      powerd_mediator{new PowerdMediator()},
      config{config},
      power_off_alarm{config->the_main_loop()->notify_in(power_off_timeout,
          std::bind(&ScreenStateHandler::power_off_alarm_notification, this))},
      dimmer_alarm{config->the_main_loop()->notify_in(dimming_timeout,
          std::bind(&ScreenStateHandler::dimmer_alarm_notification, this))},
      dbus_screen{new DBusScreen(*this)}
{
}

ScreenStateHandler::~ScreenStateHandler() = default;

bool ScreenStateHandler::handle(MirEvent const& event)
{
    if (event.type == mir_event_type_motion)
    {
        std::lock_guard<std::mutex> lock{guard};
        reset_timers_l();
        powerd_mediator->set_normal_backlight();
    }
    return false;
}

void ScreenStateHandler::set_timeouts(std::chrono::milliseconds the_power_off_timeout,
                                      std::chrono::milliseconds the_dimming_timeout)
{
    std::lock_guard<std::mutex> lock{guard};
    power_off_timeout = the_power_off_timeout;
    dimming_timeout = the_dimming_timeout;
    reset_timers_l();
}

void ScreenStateHandler::enable_inactivity_timers(bool enable)
{
    std::lock_guard<std::mutex> lock{guard};
    enable_inactivity_timers_l(enable);
}

void ScreenStateHandler::toggle_screen_power_mode(PowerStateChangeReason reason)
{
    std::lock_guard<std::mutex> lock{guard};
    MirPowerMode new_mode = (current_power_mode == MirPowerMode::mir_power_mode_on) ?
            MirPowerMode::mir_power_mode_off : MirPowerMode::mir_power_mode_on;

    set_screen_power_mode_l(new_mode, reason);
}

void ScreenStateHandler::set_screen_power_mode(MirPowerMode mode, PowerStateChangeReason reason)
{
    std::lock_guard<std::mutex> lock{guard};
    set_screen_power_mode_l(mode, reason);
}

void ScreenStateHandler::keep_display_on(bool on)
{
    std::lock_guard<std::mutex> lock{guard};
    restart_timers = !on;
    enable_inactivity_timers_l(!on);

    if (on)
        set_screen_power_mode_l(MirPowerMode::mir_power_mode_on, PowerStateChangeReason::unknown);
}

void ScreenStateHandler::set_brightness(int brightness)
{
    std::lock_guard<std::mutex> lock{guard};
    powerd_mediator->set_brightness(brightness);
}

void ScreenStateHandler::enable_auto_brightness(bool enable)
{
    std::lock_guard<std::mutex> lock{guard};
    powerd_mediator->enable_auto_brightness(enable);
}

void ScreenStateHandler::set_screen_power_mode_l(MirPowerMode mode, PowerStateChangeReason reason)
{
    if (mode == MirPowerMode::mir_power_mode_on)
    {
        /* The screen may be dim, but on - make sure to reset backlight */
        if (current_power_mode == MirPowerMode::mir_power_mode_on)
            powerd_mediator->set_normal_backlight();
        configure_display_l(mode, reason);
        reset_timers_l();
    }
    else
    {
        cancel_timers_l();
        configure_display_l(mode, reason);
    }
}

void ScreenStateHandler::configure_display_l(MirPowerMode mode, PowerStateChangeReason reason)
{
    if (current_power_mode == mode)
        return;

    std::shared_ptr<mg::Display> display = config->the_display();
    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();
    std::shared_ptr<mc::Compositor> compositor = config->the_compositor();

    displayConfig->for_each_output(
        [&](const mg::UserDisplayConfigurationOutput displayConfigOutput) {
            displayConfigOutput.power_mode = mode;
        }
    );
    
    compositor->stop();

    bool const power_on = mode == MirPowerMode::mir_power_mode_on;
    if (power_on)
    {
        //Some devices do not turn screen on properly from suspend mode
        powerd_mediator->disable_suspend();
    }

    display->configure(*displayConfig.get());

    if (power_on)
    {
        powerd_mediator->wait_for_state(power_on);
        compositor->start();
        powerd_mediator->set_normal_backlight();
    }
    else
    {
        powerd_mediator->turn_off_backlight();
    }

    current_power_mode = mode;

    dbus_screen->emit_power_state_change(mode, reason);

    if (!power_on) {
        powerd_mediator->allow_suspend();
        powerd_mediator->wait_for_state(power_on);
    }
}

void ScreenStateHandler::cancel_timers_l()
{
    power_off_alarm->cancel();
    dimmer_alarm->cancel();
}

void ScreenStateHandler::reset_timers_l()
{
    if (restart_timers && current_power_mode != MirPowerMode::mir_power_mode_off)
    {
        power_off_alarm->reschedule_in(power_off_timeout);
        dimmer_alarm->reschedule_in(dimming_timeout);
    }
}

void ScreenStateHandler::enable_inactivity_timers_l(bool enable)
{
    if (enable)
        reset_timers_l();
    else
        cancel_timers_l();
}

void ScreenStateHandler::power_off_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    configure_display_l(MirPowerMode::mir_power_mode_off, PowerStateChangeReason::inactivity);
}

void ScreenStateHandler::dimmer_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    powerd_mediator->set_dim_backlight();
}
