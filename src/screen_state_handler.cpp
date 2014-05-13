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

#include <mir/main_loop.h>
#include <mir/time/timer.h>
#include <mir/compositor/compositor.h>
#include <mir/default_server_configuration.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>

#include "screen_state_handler.h"
#include "dbus_screen.h"
#include "dbus_powerkey.h"
#include "powerd_mediator.h"

namespace mi = mir::input;
namespace mc = mir::compositor;
namespace mg = mir::graphics;

ScreenStateHandler::ScreenStateHandler(std::shared_ptr<mir::DefaultServerConfiguration> const& config,
                                       std::chrono::milliseconds power_off_timeout,
                                       std::chrono::milliseconds dimmer_timeout,
                                       std::chrono::milliseconds power_key_held_timeout)
    : current_power_mode{MirPowerMode::mir_power_mode_on},
      long_press_detected{false},
      power_off_timeout{power_off_timeout},
      dimming_timeout{dimmer_timeout},
      power_key_long_press_timeout{power_key_held_timeout},
      powerd_mediator{new PowerdMediator()},
      config{config},
      power_off_alarm{config->the_main_loop()->notify_in(power_off_timeout,
          std::bind(&ScreenStateHandler::power_off_alarm_notification, this))},
      dimmer_alarm{config->the_main_loop()->notify_in(dimming_timeout,
          std::bind(&ScreenStateHandler::dimmer_alarm_notification, this))},
      long_press_alarm{config->the_main_loop()->notify_in(power_key_long_press_timeout,
          std::bind(&ScreenStateHandler::long_press_alarm_notification, this))},
      dbus_screen{new DBusScreen([this](MirPowerMode m){
          std::lock_guard<std::mutex> lock{guard};
          set_screen_power_mode_l(m);})},
      dbus_powerkey{new DBusPowerKey()}
{

    /* TODO: change to using create_alarm once the api is added */
    long_press_alarm->cancel();
}

ScreenStateHandler::~ScreenStateHandler() = default;

bool ScreenStateHandler::handle(MirEvent const& event)
{
    static const int32_t POWER_KEY_CODE = 26;

    if (event.type == mir_event_type_key &&
        event.key.key_code == POWER_KEY_CODE)
    {
        if (event.key.action == mir_key_action_down)
            power_key_down();
        else if (event.key.action == mir_key_action_up)
            power_key_up();
    }
    else if (event.type == mir_event_type_motion)
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
}

void ScreenStateHandler::set_screen_power_mode_l(MirPowerMode mode)
{
    if (mode == MirPowerMode::mir_power_mode_on)
    {
        configure_display_l(mode);
        reset_timers_l();
    }
    else
    {
        cancel_timers_l();
        configure_display_l(mode);
    }
}

void ScreenStateHandler::toggle_screen_power_mode_l()
{
    MirPowerMode new_mode = (current_power_mode == MirPowerMode::mir_power_mode_on) ?
            MirPowerMode::mir_power_mode_off : MirPowerMode::mir_power_mode_on;

    set_screen_power_mode_l(new_mode);
}

void ScreenStateHandler::configure_display_l(MirPowerMode mode)
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
    display->configure(*displayConfig.get());
    //TODO: once the new swap unblock solution lands in mir
    //only start compositor on power on state
    compositor->start();

    if (mode == MirPowerMode::mir_power_mode_on)
    {
        powerd_mediator->set_normal_backlight();
    }
    else
    {
        powerd_mediator->turn_off_backlight();
    }

    current_power_mode = mode;
    powerd_mediator->set_sys_state_for(mode);
    dbus_screen->emit_power_state_change(mode);
}

void ScreenStateHandler::cancel_timers_l()
{
    power_off_alarm->cancel();
    dimmer_alarm->cancel();
}

void ScreenStateHandler::reset_timers_l()
{
    if (current_power_mode != MirPowerMode::mir_power_mode_off)
    {
        power_off_alarm->reschedule_in(power_off_timeout);
        dimmer_alarm->reschedule_in(dimming_timeout);
    }
}

void ScreenStateHandler::power_off_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    configure_display_l(MirPowerMode::mir_power_mode_off);
}

void ScreenStateHandler::dimmer_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    powerd_mediator->set_dim_backlight();
}

void ScreenStateHandler::long_press_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    long_press_detected = true;
    dbus_powerkey->emit_power_off_request();
    reset_timers_l();
}

void ScreenStateHandler::power_key_down()
{
    std::lock_guard<std::mutex> lock{guard};
    cancel_timers_l();
    long_press_detected = false;
    long_press_alarm->reschedule_in(power_key_long_press_timeout);
}

void ScreenStateHandler::power_key_up()
{
    std::lock_guard<std::mutex> lock{guard};
    long_press_alarm->cancel();

    if (!long_press_detected)
    {
        toggle_screen_power_mode_l();
    }
}
