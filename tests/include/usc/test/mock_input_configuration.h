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
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef USC_TEST_MOCK_INPUT_CONFIGURATION_H_
#define USC_TEST_MOCK_INPUT_CONFIGURATION_H_

#include "src/input_configuration.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace usc
{
namespace test
{
struct MockInputConfiguration : usc::InputConfiguration
{
public:
    MOCK_METHOD1(set_mouse_primary_button, void(int32_t));
    MOCK_METHOD1(set_mouse_cursor_speed, void(double));
    MOCK_METHOD1(set_mouse_scroll_speed, void(double));
    MOCK_METHOD1(set_touchpad_primary_button, void(int32_t));
    MOCK_METHOD1(set_touchpad_cursor_speed, void(double));
    MOCK_METHOD1(set_touchpad_scroll_speed, void(double));
    MOCK_METHOD1(set_two_finger_scroll, void(bool));
    MOCK_METHOD1(set_tap_to_click, void(bool));
    MOCK_METHOD1(set_disable_touchpad_with_mouse, void(bool));
    MOCK_METHOD1(set_disable_touchpad_while_typing, void(bool));
};
}
}

#endif
