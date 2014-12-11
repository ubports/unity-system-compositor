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

#include "powerkey_handler.h"
#include "screen_state_handler.h"
#include "power_state_change_reason.h"

#include <mir/time/timer.h>

#include <cstdio>
#include "dbus_screen.h"
#include "powerd_mediator.h"

namespace mi = mir::input;

PowerKeyHandler::PowerKeyHandler(mir::time::Timer& timer,
                                 std::chrono::milliseconds power_key_ignore_timeout,
                                 std::chrono::milliseconds shutdown_timeout,
                                 ScreenStateHandler& screen_state_handler)
    : long_press_detected{false},
      screen_state_handler{&screen_state_handler},
      power_key_ignore_timeout{power_key_ignore_timeout},
      shutdown_timeout{shutdown_timeout},
      shutdown_alarm{timer.create_alarm([this]{ shutdown_alarm_notification(); })},
      long_press_alarm{timer.create_alarm([this]{ long_press_notification(); })}
{
}

PowerKeyHandler::~PowerKeyHandler() = default;

bool PowerKeyHandler::handle(MirEvent const& event)
{
    static const int32_t POWER_KEY_CODE = 26;
    
    if (mir_event_get_type(&event) != mir_event_type_input)
        return false;
    auto input_event = mir_event_get_input_event(&event);
    if (mir_input_event_get_type(input_event) != mir_input_event_type_key)
        return false;
    auto kev = mir_input_event_get_key_input_event(input_event);
    if (mir_key_input_event_get_key_code(kev) != POWER_KEY_CODE)
        return false;

    auto action = mir_key_input_event_get_action(kev);
    if (action == mir_key_input_event_action_down)
        power_key_down();
    else if (action == mir_key_input_event_action_up)
        power_key_up();

    return false;
}

void PowerKeyHandler::power_key_down()
{
    std::lock_guard<std::mutex> lock{guard};
    screen_state_handler->enable_inactivity_timers(false);
    long_press_detected = false;
    long_press_alarm->reschedule_in(power_key_ignore_timeout);
    shutdown_alarm->reschedule_in(shutdown_timeout);
}

void PowerKeyHandler::power_key_up()
{
    std::lock_guard<std::mutex> lock{guard};
    shutdown_alarm->cancel();
    long_press_alarm->cancel();
    if (!long_press_detected)
    {
        screen_state_handler->toggle_screen_power_mode(PowerStateChangeReason::power_key);
    }
}

void PowerKeyHandler::shutdown_alarm_notification()
{
    screen_state_handler->set_screen_power_mode(
        MirPowerMode::mir_power_mode_off, PowerStateChangeReason::power_key);
    system("shutdown -P now");
}

void PowerKeyHandler::long_press_notification()
{
    screen_state_handler->set_screen_power_mode(
        MirPowerMode::mir_power_mode_on, PowerStateChangeReason::power_key);
    long_press_detected = true;
}
