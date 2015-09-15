/*
 * Copyright © 2015 Canonical Ltd.
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

#include "mir_input_configuration.h"

// just a stub at the moment, this implementation will be supposed to
// observe available input devices and apply settings to it.


void usc::MirInputConfiguration::set_mouse_primary_button(int32_t button)
{
    mouse_primary_button = button;
}

void usc::MirInputConfiguration::set_mouse_cursor_speed(double speed)
{
    mouse_cursor_speed = speed;
}

void usc::MirInputConfiguration::set_mouse_scroll_speed(double speed)
{
    mouse_scroll_speed = speed;
}

void usc::MirInputConfiguration::set_touchpad_primary_button(int32_t button)
{
    touchpad_primary_button = button;
}

void usc::MirInputConfiguration::set_touchpad_cursor_speed(double speed)
{
    touchpad_cursor_speed = speed;
}

void usc::MirInputConfiguration::set_touchpad_scroll_speed(double speed)
{
    touchpad_scroll_speed = speed;
}

void usc::MirInputConfiguration::set_two_finger_scroll(bool enable)
{
    two_finger_scroll = enable;
}

void usc::MirInputConfiguration::set_tap_to_click(bool enable)
{
    tap_to_click = enable;
}

void usc::MirInputConfiguration::set_disable_touchpad_while_typing(bool enable)
{
    disable_while_typing = enable;
}

void usc::MirInputConfiguration::set_disable_touchpad_with_mouse(bool enable)
{
    disable_with_mouse = enable;
}

