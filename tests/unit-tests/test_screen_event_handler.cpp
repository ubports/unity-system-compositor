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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/screen_event_handler.h"
#include "src/power_button_event_sink.h"
#include "src/user_activity_event_sink.h"

#include "advanceable_timer.h"
#include "fake_shared.h"

#include <mir/events/event_builders.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "linux/input.h"

using namespace testing;
using namespace std::chrono_literals;

namespace
{

struct MockPowerButtonEventSink : usc::PowerButtonEventSink
{
    MOCK_METHOD0(notify_press, void());
    MOCK_METHOD0(notify_release, void());
};

struct MockUserActivityEventSink : usc::UserActivityEventSink
{
    MOCK_METHOD0(notify_activity_changing_power_state, void());
    MOCK_METHOD0(notify_activity_extending_power_state, void());
};

struct AScreenEventHandler : testing::Test
{
    void press_power_key()
    {
        screen_event_handler.handle(*power_key_down_event);
    }

    void release_power_key()
    {
        screen_event_handler.handle(*power_key_up_event);
    }

    void press_a_key()
    {
        screen_event_handler.handle(*another_key_down_event);
    }

    void release_a_key()
    {
        screen_event_handler.handle(*another_key_up_event);
    }

    void repeat_a_key()
    {
        screen_event_handler.handle(*another_key_repeat_event);
    }

    void press_volume_keys()
    {
        screen_event_handler.handle(*vol_plus_key_down_event);
        screen_event_handler.handle(*vol_plus_key_up_event);
        screen_event_handler.handle(*vol_minus_key_down_event);
        screen_event_handler.handle(*vol_minus_key_up_event);
    }

    void touch_screen()
    {
        screen_event_handler.handle(*touch_event);
    }

    void move_pointer()
    {
        screen_event_handler.handle(*pointer_event);
    }

    mir::EventUPtr power_key_down_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_down,
        0, KEY_POWER, mir_input_event_modifier_none);

    mir::EventUPtr power_key_up_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_up,
        0, KEY_POWER, mir_input_event_modifier_none);

    mir::EventUPtr vol_plus_key_down_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_down,
        0, KEY_VOLUMEUP, mir_input_event_modifier_none);

    mir::EventUPtr vol_plus_key_up_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_up,
        0, KEY_VOLUMEUP, mir_input_event_modifier_none);

    mir::EventUPtr vol_minus_key_down_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_down,
        0, KEY_VOLUMEDOWN, mir_input_event_modifier_none);

    mir::EventUPtr vol_minus_key_up_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_up,
        0, KEY_VOLUMEDOWN, mir_input_event_modifier_none);

    mir::EventUPtr another_key_down_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_down,
        0, KEY_A, mir_input_event_modifier_none);

    mir::EventUPtr another_key_up_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_keyboard_action_up,
        0, KEY_A, mir_input_event_modifier_none);

    mir::EventUPtr another_key_repeat_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
        std::vector<uint8_t>{}, mir_keyboard_action_repeat,
        0, KEY_A, mir_input_event_modifier_none);

    mir::EventUPtr touch_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_input_event_modifier_none);

    mir::EventUPtr pointer_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_input_event_modifier_none,
        mir_pointer_action_motion,
        {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    AdvanceableTimer timer;
    NiceMock<MockPowerButtonEventSink> mock_power_button_event_sink;
    NiceMock<MockUserActivityEventSink> mock_user_activity_event_sink;
    usc::ScreenEventHandler screen_event_handler{
        usc::test::fake_shared(mock_power_button_event_sink),
        usc::test::fake_shared(mock_user_activity_event_sink),
        usc::test::fake_shared(timer)};
};

}

TEST_F(AScreenEventHandler, notifies_of_power_key_press)
{
    EXPECT_CALL(mock_power_button_event_sink, notify_press());

    press_power_key();
}

TEST_F(AScreenEventHandler, notifies_of_power_key_release)
{
    EXPECT_CALL(mock_power_button_event_sink, notify_release());

    release_power_key();
}

TEST_F(AScreenEventHandler, notifies_of_activity_extending_power_state_for_touch_event)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_extending_power_state());

    touch_screen();
}

TEST_F(AScreenEventHandler, notifies_of_activity_changing_power_state_for_pointer_event)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_changing_power_state());

    move_pointer();
}

TEST_F(AScreenEventHandler, notifies_of_activity_changing_power_state_for_key_press)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_changing_power_state());

    press_a_key();
}

TEST_F(AScreenEventHandler, notifies_of_activity_extending_power_state_for_key_release)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_extending_power_state());

    release_a_key();
}

TEST_F(AScreenEventHandler, notifies_of_activity_extending_power_state_for_key_repeat)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_extending_power_state());

    repeat_a_key();
}

TEST_F(AScreenEventHandler, does_not_notify_of_activity_for_volume_keys)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_changing_power_state()).Times(0);
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_extending_power_state()).Times(0);

    press_volume_keys();
}

TEST_F(AScreenEventHandler, does_not_notify_of_activity_soon_after_last_notification)
{
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_extending_power_state()).Times(1);
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_changing_power_state()).Times(1);

    touch_screen();
    press_a_key();
    touch_screen();
    press_a_key();
    Mock::VerifyAndClearExpectations(&mock_user_activity_event_sink);

    timer.advance_by(500ms);

    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_extending_power_state()).Times(1);
    EXPECT_CALL(mock_user_activity_event_sink,
                notify_activity_changing_power_state()).Times(1);

    touch_screen();
    press_a_key();
}

TEST_F(AScreenEventHandler, passes_through_all_handled_events)
{
    EXPECT_FALSE(screen_event_handler.handle(*power_key_down_event));
    EXPECT_FALSE(screen_event_handler.handle(*power_key_up_event));
    EXPECT_FALSE(screen_event_handler.handle(*touch_event));
    EXPECT_FALSE(screen_event_handler.handle(*pointer_event));
}
