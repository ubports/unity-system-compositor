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

#ifndef USC_INPUT_CONFIGURATION_H_
#define USC_INPUT_CONFIGURATION_H_

#include <cstdint>

namespace usc
{
class InputConfiguration
{
public:
    virtual ~InputConfiguration() = default;

    virtual void set_mouse_primary_button(int32_t button) = 0;
    virtual void set_mouse_cursor_speed(double speed) = 0;
    virtual void set_mouse_scroll_speed(double speed) = 0;
    virtual void set_touchpad_primary_button(int32_t button) = 0;
    virtual void set_touchpad_cursor_speed(double speed) = 0;
    virtual void set_touchpad_scroll_speed(double speed) = 0;
    virtual void set_two_finger_scroll(bool enable) = 0;
    virtual void set_tap_to_click(bool enable) = 0;
    virtual void set_disable_touchpad_while_typing(bool enable) = 0;
    virtual void set_disable_touchpad_with_mouse(bool enable) = 0;

protected:
    InputConfiguration() = default;
    InputConfiguration(InputConfiguration const&) = delete;
    InputConfiguration& operator=(InputConfiguration const&) = delete;
};


}

#endif
