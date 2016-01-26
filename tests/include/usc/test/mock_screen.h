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

#ifndef USC_TEST_MOCK_SCREEN_H_
#define USC_TEST_MOCK_SCREEN_H_

#include "src/screen.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
namespace usc
{
namespace test
{

struct MockScreen : usc::Screen
{
    MOCK_METHOD1(enable_inactivity_timers, void(bool enable));
    MOCK_METHOD0(keep_display_on_temporarily, void());

    MOCK_METHOD0(get_screen_power_mode, MirPowerMode());
    MOCK_METHOD2(set_screen_power_mode, void(MirPowerMode mode, PowerStateChangeReason reason));
    MOCK_METHOD1(keep_display_on, void(bool on));
    MOCK_METHOD1(set_brightness, void(int brightness));
    MOCK_METHOD1(enable_auto_brightness, void(bool enable));
    MOCK_METHOD2(set_inactivity_timeouts, void(int power_off_timeout, int dimmer_timeout));

    MOCK_METHOD1(set_touch_visualization_enabled, void(bool enabled));

    void register_power_state_change_handler(
        usc::PowerStateChangeHandler const& handler)
    {
        power_state_change_handler = handler;
    }

    usc::PowerStateChangeHandler power_state_change_handler;
};

}
}

#endif
