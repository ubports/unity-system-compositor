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
#include "power_button_event_sink.h"
#include "user_activity_event_sink.h"

#include <mir_toolkit/events/input/input_event.h>
#include <mir/time/alarm_factory.h>

#include "linux/input.h"
#include <cstdio>

usc::ScreenEventHandler::ScreenEventHandler(
    std::shared_ptr<PowerButtonEventSink> const& power_button_event_sink,
    std::shared_ptr<UserActivityEventSink> const& user_activity_event_sink,
    std::shared_ptr<mir::time::AlarmFactory> const& alarm_factory)
    : power_button_event_sink{power_button_event_sink},
      user_activity_event_sink{user_activity_event_sink},
      alarm_factory{alarm_factory},
      activity_changing_power_state_alarm{
          alarm_factory->create_alarm(
              [this] { activity_changing_power_state_alarm_handler(); })},
      activity_extending_power_state_alarm{
          alarm_factory->create_alarm(
              [this] { activity_extending_power_state_alarm_handler(); })} 
{
}

usc::ScreenEventHandler::~ScreenEventHandler() = default;

bool usc::ScreenEventHandler::handle(MirEvent const& event)
{
    if (mir_event_get_type(&event) != mir_event_type_input)
        return false;

    auto const input_event = mir_event_get_input_event(&event);
    auto const input_event_type = mir_input_event_get_type(input_event);

    if (input_event_type == mir_input_event_type_key)
    {
        auto const kev = mir_input_event_get_keyboard_event(input_event);
        if (mir_keyboard_event_scan_code(kev) == KEY_POWER)
        {
            auto const action = mir_keyboard_event_action(kev);
            if (action == mir_keyboard_action_down)
                power_button_event_sink->notify_press();
            else if (action == mir_keyboard_action_up)
                power_button_event_sink->notify_release();
        }
        // we might want to come up with a whole range of media player related keys
        else if (mir_keyboard_event_scan_code(kev) == KEY_VOLUMEDOWN||
                 mir_keyboard_event_scan_code(kev) == KEY_VOLUMEUP)
        {
            // do not keep display on when interacting with media player
        }
        else
        {
            notify_activity_changing_power_state();
        }
    }
    else if (input_event_type == mir_input_event_type_touch)
    {
        notify_activity_extending_power_state();
    }
    else if (input_event_type == mir_input_event_type_pointer)
    {
        notify_activity_changing_power_state();
    }

    return false;
}

void usc::ScreenEventHandler::notify_activity_changing_power_state()
{
    std::lock_guard<std::mutex> lock{alarm_mutex};

    if (!activity_changing_power_state_alarm_pending)
    {
        user_activity_event_sink->notify_activity_changing_power_state();
        activity_changing_power_state_alarm->reschedule_in(event_period);
        activity_changing_power_state_alarm_pending = true;
        have_activity_changing_power_state = false;
    }
    else
    {
        have_activity_changing_power_state = true;
    }
}

void usc::ScreenEventHandler::notify_activity_extending_power_state()
{
    std::lock_guard<std::mutex> lock{alarm_mutex};

    if (!activity_extending_power_state_alarm_pending)
    {
        user_activity_event_sink->notify_activity_extending_power_state();
        activity_extending_power_state_alarm->reschedule_in(event_period);
        activity_extending_power_state_alarm_pending = true;
        have_activity_extending_power_state = false;
    }
    else
    {
        have_activity_extending_power_state = true;
    }
}

void usc::ScreenEventHandler::activity_changing_power_state_alarm_handler()
{
    std::lock_guard<std::mutex> lock{alarm_mutex};

    activity_changing_power_state_alarm_pending = false;
    if (have_activity_changing_power_state)
    {
        user_activity_event_sink->notify_activity_changing_power_state();
        have_activity_changing_power_state = false;
    }
}

void usc::ScreenEventHandler::activity_extending_power_state_alarm_handler()
{
    std::lock_guard<std::mutex> lock{alarm_mutex};

    activity_extending_power_state_alarm_pending = false;
    if (have_activity_extending_power_state)
    {
        user_activity_event_sink->notify_activity_extending_power_state();
        have_activity_extending_power_state = false;
    }
}
