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
#include "src/power_state_change_reason.h"
#include "src/screen.h"

#include "advanceable_timer.h"
#include "fake_shared.h"

#include <mir/events/event_builders.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "linux/input.h"
#include <atomic>

namespace
{

struct MockScreen : usc::Screen
{
    MOCK_METHOD1(enable_inactivity_timers, void(bool enable));
    MOCK_METHOD0(keep_display_on_temporarily, void());

    MirPowerMode get_screen_power_mode() override {return mock_mode;}
    MOCK_METHOD2(set_screen_power_mode, void(MirPowerMode mode, PowerStateChangeReason reason));
    MOCK_METHOD1(keep_display_on, void(bool on));
    MOCK_METHOD1(set_brightness, void(int brightness));
    MOCK_METHOD1(enable_auto_brightness, void(bool enable));
    MOCK_METHOD2(set_inactivity_timeouts, void(int power_off_timeout, int dimmer_timeout));

    MOCK_METHOD1(set_touch_visualization_enabled, void(bool enabled));
    MOCK_METHOD1(register_power_state_change_handler, void(
                 usc::PowerStateChangeHandler const& handler));

    MirPowerMode mock_mode = MirPowerMode::mir_power_mode_off;
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

    mir::EventUPtr touch_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_input_event_modifier_none);

    mir::EventUPtr pointer_event = mir::events::make_event(
        MirInputDeviceId{1}, std::chrono::nanoseconds(0),
	    std::vector<uint8_t>{}, mir_input_event_modifier_none,
        mir_pointer_action_motion,
        {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    AdvanceableTimer timer;
    std::chrono::milliseconds const power_key_ignore_timeout{5000};
    std::chrono::milliseconds const shutdown_timeout{10000};
    std::chrono::milliseconds const normal_press_duration{100};
    testing::NiceMock<MockScreen> mock_screen;
    std::atomic<bool> shutdown_called{false};
    usc::ScreenEventHandler screen_event_handler{
        usc::test::fake_shared(mock_screen),
        usc::test::fake_shared(timer),
        power_key_ignore_timeout,
        shutdown_timeout,
        [&] { shutdown_called = true; }};
};

}

TEST_F(AScreenEventHandler, turns_screen_on_immediately_on_press)
{
    auto const long_press_duration = power_key_ignore_timeout;

    EXPECT_CALL(mock_screen,
                set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                      PowerStateChangeReason::power_key));

    press_power_key();
}

TEST_F(AScreenEventHandler, shuts_down_system_when_power_key_pressed_for_long_enough)
{
    using namespace testing;

    std::chrono::milliseconds const one_ms{1};

    press_power_key();

    timer.advance_by(shutdown_timeout - one_ms);
    ASSERT_FALSE(shutdown_called);

    timer.advance_by(one_ms);
    ASSERT_TRUE(shutdown_called);
}

TEST_F(AScreenEventHandler, turns_screen_off_when_shutting_down)
{
    press_power_key();

    testing::InSequence s;

    // First, screen turns on from long press
    EXPECT_CALL(mock_screen,
                set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                      PowerStateChangeReason::power_key));
    EXPECT_CALL(mock_screen,
                set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                      PowerStateChangeReason::power_key));
    timer.advance_by(shutdown_timeout);
}

TEST_F(AScreenEventHandler, keeps_display_on_temporarily_on_touch_event)
{
    EXPECT_CALL(mock_screen, keep_display_on_temporarily());

    touch_screen();
}

TEST_F(AScreenEventHandler, keeps_display_on_temporarily_on_pointer_event)
{
    EXPECT_CALL(mock_screen, keep_display_on_temporarily());

    move_pointer();
}

TEST_F(AScreenEventHandler, keeps_display_on_temporarily_on_other_keys)
{
    EXPECT_CALL(mock_screen, keep_display_on_temporarily());

    press_a_key();
}

TEST_F(AScreenEventHandler, does_not_keep_display_on_for_volume_keys)
{
    EXPECT_CALL(mock_screen, keep_display_on_temporarily()).Times(0);

    press_volume_keys();
}

TEST_F(AScreenEventHandler, sets_screen_mode_off_normal_press_release)
{
    EXPECT_CALL(mock_screen,
                set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                      PowerStateChangeReason::power_key));

    mock_screen.mock_mode = MirPowerMode::mir_power_mode_on;
    press_power_key();
    timer.advance_by(normal_press_duration);
    release_power_key();
}

TEST_F(AScreenEventHandler, does_not_set_screen_mode_off_long_press_release)
{
    using namespace testing;

    auto const long_press_duration = power_key_ignore_timeout;

    EXPECT_CALL(mock_screen,
                set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                      PowerStateChangeReason::power_key));
    EXPECT_CALL(mock_screen,
                set_screen_power_mode(MirPowerMode::mir_power_mode_off, _))
        .Times(0);

    mock_screen.mock_mode = MirPowerMode::mir_power_mode_on;
    press_power_key();
    timer.advance_by(long_press_duration);
    release_power_key();
}

TEST_F(AScreenEventHandler, passes_through_all_handled_events)
{
    using namespace testing;

    EXPECT_FALSE(screen_event_handler.handle(*power_key_down_event));
    EXPECT_FALSE(screen_event_handler.handle(*power_key_up_event));
    EXPECT_FALSE(screen_event_handler.handle(*touch_event));
    EXPECT_FALSE(screen_event_handler.handle(*pointer_event));
}

TEST_F(AScreenEventHandler, disables_inactivity_timers_on_power_key_down)
{
    EXPECT_CALL(mock_screen, enable_inactivity_timers(false));

    press_power_key();
}

TEST_F(AScreenEventHandler, enables_inactivity_timers_on_power_key_up_when_turning_screen_on)
{
    press_power_key();
    timer.advance_by(normal_press_duration);

    EXPECT_CALL(mock_screen, enable_inactivity_timers(true));
    release_power_key();
}
