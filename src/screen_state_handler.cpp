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
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>
#include <mir/input/touch_visualizer.h>

#include <cstdio>
#include "dbus_screen.h"
#include "dbus_screen_observer.h"
#include "powerd_mediator.h"
#include "power_state_change_reason.h"
#include "server.h"

namespace mi = mir::input;
namespace mc = mir::compositor;
namespace mg = mir::graphics;

ScreenStateHandler::ScreenStateHandler(std::shared_ptr<usc::Server> const& server,
                                       std::chrono::milliseconds poweroff_timeout,
                                       std::chrono::milliseconds dimmer_timeout)
    : current_power_mode{MirPowerMode::mir_power_mode_on},
      restart_timers{true},
      power_off_timeout{poweroff_timeout},
      dimming_timeout{dimmer_timeout},
      powerd_mediator{new PowerdMediator()},
      server{server},
      power_off_alarm{server->the_main_loop()->create_alarm(
              std::bind(&ScreenStateHandler::power_off_alarm_notification, this))},
      dimmer_alarm{server->the_main_loop()->create_alarm(
              std::bind(&ScreenStateHandler::dimmer_alarm_notification, this))},
      dbus_screen{new DBusScreen(*this)}
{
    /* Make sure the compositor is running before the handler runs. */
    server->the_compositor()->start();
    reset_timers_l();
}

ScreenStateHandler::~ScreenStateHandler() = default;

bool ScreenStateHandler::handle(MirEvent const& event)
{
    if (event.type == mir_event_type_motion)
    {
        std::lock_guard<std::mutex> lock{guard};
        reset_timers_l();
        if (current_power_mode == MirPowerMode::mir_power_mode_on)
            powerd_mediator->set_normal_backlight();
    }
    return false;
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

void ScreenStateHandler::set_inactivity_timeouts(int raw_poweroff_timeout, int raw_dimmer_timeout)
{
    std::lock_guard<std::mutex> lock{guard};

    std::chrono::seconds the_power_off_timeout{raw_poweroff_timeout};
    std::chrono::seconds the_dimming_timeout{raw_dimmer_timeout};

    if (raw_poweroff_timeout >= 0)
        power_off_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(the_power_off_timeout);

    if (raw_dimmer_timeout >= 0)
        dimming_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(the_dimming_timeout);

    cancel_timers_l();
    reset_timers_l();
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

    std::shared_ptr<mg::Display> display = server->the_display();
    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();
    std::shared_ptr<mc::Compositor> compositor = server->the_compositor();

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
    else
    {
        powerd_mediator->turn_off_backlight();
    }

    display->configure(*displayConfig.get());

    if (power_on)
    {
        compositor->start();
        powerd_mediator->set_normal_backlight();
    }

    current_power_mode = mode;

    dbus_screen->emit_power_state_change(mode, reason);

    if (!power_on)
        powerd_mediator->allow_suspend();
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
        if (power_off_timeout.count() > 0)
            power_off_alarm->reschedule_in(power_off_timeout);

        if (dimming_timeout.count() > 0)
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

void ScreenStateHandler::set_touch_visualization_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock{guard};
    
    auto visualizer = server->the_touch_visualizer();
    if (enabled)
        visualizer->enable();
    else
        visualizer->disable();
}
