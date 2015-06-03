/*
 * Copyright © 2014-2015 Canonical Ltd.
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

#include "screen_event_handler.h"
#include "screen.h"
#include "power_state_change_reason.h"

#include <mir/time/alarm_factory.h>
#include <mir_toolkit/events/input/input_event.h>

#include <cstdio>

usc::ScreenEventHandler::ScreenEventHandler(
    std::shared_ptr<Screen> const& screen,
    std::shared_ptr<mir::time::AlarmFactory> const& alarm_factory,
    std::chrono::milliseconds power_key_ignore_timeout,
    std::chrono::milliseconds shutdown_timeout,
    std::function<void()> const& shutdown)
    : screen{screen},
      alarm_factory{alarm_factory},
      power_key_ignore_timeout{power_key_ignore_timeout},
      shutdown_timeout{shutdown_timeout},
      shutdown{shutdown},
      long_press_detected{false},
      mode_at_press_start{MirPowerMode::mir_power_mode_off},
      shutdown_alarm{alarm_factory->create_alarm([this]{ shutdown_alarm_notification(); })},
      long_press_alarm{alarm_factory->create_alarm([this]{ long_press_notification(); })}
{
}

usc::ScreenEventHandler::~ScreenEventHandler() = default;

bool usc::ScreenEventHandler::handle(MirEvent const& event)
{
    static const int32_t POWER_KEY_CODE = 26;

    if (mir_event_get_type(&event) != mir_event_type_input)
        return false;

    auto const input_event = mir_event_get_input_event(&event);
    auto const input_event_type = mir_input_event_get_type(input_event);

    if (input_event_type == mir_input_event_type_key)
    {
        auto const kev = mir_input_event_get_keyboard_event(input_event);
        if (mir_keyboard_event_key_code(kev) != POWER_KEY_CODE)
            return false;

        auto const action = mir_keyboard_event_action(kev);
        if (action == mir_keyboard_action_down)
            power_key_down();
        else if (action == mir_keyboard_action_up)
            power_key_up();
    }
    else if (input_event_type == mir_input_event_type_touch ||
             input_event_type == mir_input_event_type_pointer)
    {
        std::lock_guard<std::mutex> lock{guard};
        screen->keep_display_on_temporarily();
    }

    return false;
}

void usc::ScreenEventHandler::power_key_down()
{
    std::lock_guard<std::mutex> lock{guard};

    mode_at_press_start = screen->get_screen_power_mode();
    if (mode_at_press_start == MirPowerMode::mir_power_mode_off)
    {
        screen->set_screen_power_mode(
            MirPowerMode::mir_power_mode_on, PowerStateChangeReason::power_key);
    }

    screen->enable_inactivity_timers(false);
    long_press_detected = false;
    long_press_alarm->reschedule_in(power_key_ignore_timeout);
    shutdown_alarm->reschedule_in(shutdown_timeout);
}

void usc::ScreenEventHandler::power_key_up()
{
    std::lock_guard<std::mutex> lock{guard};
    shutdown_alarm->cancel();
    long_press_alarm->cancel();

    if (mode_at_press_start == MirPowerMode::mir_power_mode_on && !long_press_detected)
    {
        screen->set_screen_power_mode(
            MirPowerMode::mir_power_mode_off, PowerStateChangeReason::power_key);
    }
}

void usc::ScreenEventHandler::shutdown_alarm_notification()
{
    screen->set_screen_power_mode(
        MirPowerMode::mir_power_mode_off, PowerStateChangeReason::power_key);
    shutdown();
}

void usc::ScreenEventHandler::long_press_notification()
{
    // We know the screen is already on after power_key_down(), but we turn the
    // screen on here to ensure that it is also at full brightness for the
    // presumed system power dialog that will appear.
    screen->set_screen_power_mode(
        MirPowerMode::mir_power_mode_on, PowerStateChangeReason::power_key);
    long_press_detected = true;
}
