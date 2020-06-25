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
#include "gesture_event_sink.h"
#include "power_button_event_sink.h"
#include "user_activity_event_sink.h"
#include "clock.h"

#include <mir_toolkit/events/input/input_event.h>

#include "linux/input.h"
#include <cstdio>

usc::ScreenEventHandler::ScreenEventHandler(
    std::shared_ptr<GestureEventSink> const& gesture_event_sink,
    std::shared_ptr<PowerButtonEventSink> const& power_button_event_sink,
    std::shared_ptr<UserActivityEventSink> const& user_activity_event_sink,
    std::shared_ptr<Clock> const& clock)
    : gesture_event_sink{gesture_event_sink},
      power_button_event_sink{power_button_event_sink},
      user_activity_event_sink{user_activity_event_sink},
      clock{clock},
      last_activity_changing_power_state_event_time{-event_period},
      last_activity_extending_power_state_event_time{-event_period}
{
}

bool usc::ScreenEventHandler::handle(MirEvent const& event)
{
    if (mir_event_get_type(&event) != mir_event_type_input)
        return false;

    auto const input_event = mir_event_get_input_event(&event);
    auto const input_event_type = mir_input_event_get_type(input_event);

    if (input_event_type == mir_input_event_type_key)
    {
        auto const kev = mir_input_event_get_keyboard_event(input_event);
	int key_code = mir_keyboard_event_scan_code(kev);
        auto const action = mir_keyboard_event_action(kev);
printf("  kev code: %d\n", key_code);
        if (key_code == KEY_POWER)
        {
            if (action == mir_keyboard_action_down)
                power_button_event_sink->notify_press();
            else if (action == mir_keyboard_action_up)
                power_button_event_sink->notify_release();
        }
        // we might want to come up with a whole range of media player related keys
        // KEY_NEXTSONG..KEY_ATTENDANT_TOGGLE can come from gestures of touchpanel
        else if (   key_code == KEY_VOLUMEDOWN
                 || key_code == KEY_VOLUMEUP
                 || key_code == KEY_NEXTSONG
                 || key_code == KEY_PREVIOUSSONG
                 || key_code == KEY_PLAYPAUSE
                 || key_code == KEY_CAMERA
                 || key_code == KEY_ATTENDANT_TOGGLE)
        {
            // do not keep display on when interacting with media player
            // notify gestures on 'up'
            if (action == mir_keyboard_action_up) { 
                switch(key_code) {
                case KEY_NEXTSONG: gesture_event_sink->notify_gesture("next-song"); break;
                case KEY_PREVIOUSSONG: gesture_event_sink->notify_gesture("previous-song"); break;
                case KEY_PLAYPAUSE: gesture_event_sink->notify_gesture("play-pause"); break;
                case KEY_ATTENDANT_TOGGLE: gesture_event_sink->notify_gesture("toggle-flash"); break;
                }
            }
        }
        else if (action == mir_keyboard_action_down)
        {
            notify_activity_changing_power_state();
        }
        else
        {
            notify_activity_extending_power_state();
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
    std::lock_guard<std::mutex> lock{event_mutex};

    if (clock->now() >= last_activity_changing_power_state_event_time + event_period)
    {
        user_activity_event_sink->notify_activity_changing_power_state();
        last_activity_changing_power_state_event_time = clock->now();
    }
}

void usc::ScreenEventHandler::notify_activity_extending_power_state()
{
    std::lock_guard<std::mutex> lock{event_mutex};

    if (clock->now() >= last_activity_extending_power_state_event_time + event_period)
    {
        user_activity_event_sink->notify_activity_extending_power_state();
        last_activity_extending_power_state_event_time = clock->now();
    }
}
