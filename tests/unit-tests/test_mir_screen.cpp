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

#include "src/mir_screen.h"
#include "src/performance_booster.h"
#include "src/screen_hardware.h"
#include "src/power_state_change_reason.h"
#include "advanceable_timer.h"

#include "usc/test/mock_display.h"

#include <mir/compositor/compositor.h>
#include <mir/graphics/display_configuration.h>
#include <mir/graphics/gl_context.h>
#include <mir/input/touch_visualizer.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>

namespace mg = mir::graphics;
namespace ut = usc::test;

namespace
{

struct MockCompositor : mir::compositor::Compositor
{
    MOCK_METHOD0(start, void());
    MOCK_METHOD0(stop, void());
};

struct MockPerformanceBooster : usc::PerformanceBooster
{
    MOCK_METHOD0(enable_performance_boost_during_user_interaction, void());
    MOCK_METHOD0(disable_performance_boost_during_user_interaction, void());
};

struct MockScreenHardware : usc::ScreenHardware
{
    MOCK_METHOD0(set_dim_backlight, void());
    MOCK_METHOD0(set_normal_backlight, void());
    MOCK_METHOD0(turn_off_backlight, void());

    void change_backlight_values(int dim_brightness, int normal_brightness) override {}

    MOCK_METHOD0(allow_suspend, void());
    MOCK_METHOD0(disable_suspend, void());

    MOCK_METHOD1(enable_auto_brightness, void(bool));
    bool auto_brightness_supported() override { return true; }
    MOCK_METHOD1(set_brightness, void(int));
    int min_brightness() override { return 100; }
    int max_brightness() override { return 0; }

    enum class Proximity {near, far};

    void enable_proximity(bool enable) override
    {
        proximity_enabled = enable;
        set_proximity(current_proximity);
    }

    void set_proximity(Proximity proximity)
    {
        current_proximity = proximity;
        if (proximity_enabled)
            on_proximity_changed(current_proximity);
    }

    std::function<void(Proximity)> on_proximity_changed = [](Proximity){};
    Proximity current_proximity{Proximity::far};
    bool proximity_enabled{false};
};

struct MockTouchVisualizer : mir::input::TouchVisualizer
{
    MOCK_METHOD0(enable, void());
    MOCK_METHOD0(disable, void());

    // Visualize a given set of touches statelessly.
    void visualize_touches(std::vector<Spot> const& touches) override {}
};

struct AMirScreen : testing::Test
{
    AMirScreen()
    {
        using namespace testing;

        screen_hardware->on_proximity_changed =
            [this] (MockScreenHardware::Proximity p) { defer_proximity_event(p); };
    }

    void defer_proximity_event(MockScreenHardware::Proximity proximity)
    {
        deferred_actions.push_back(
            [this, proximity]
            {
                mir_screen.set_screen_power_mode(
                        proximity == MockScreenHardware::Proximity::far ?
                            MirPowerMode::mir_power_mode_on :
                            MirPowerMode::mir_power_mode_off,
                        PowerStateChangeReason::proximity);
            });
    }

    void process_deferred_actions()
    {
        for (auto const& a : deferred_actions)
            a();
        deferred_actions.clear();
    }

    std::vector<std::function<void(void)>> deferred_actions;

    std::chrono::seconds const power_off_timeout{60};
    std::chrono::seconds const dimmer_timeout{50};
    std::chrono::seconds const notification_power_off_timeout{15};
    std::chrono::seconds const notification_dimmer_timeout{12};
    std::chrono::seconds const call_power_off_timeout{30};
    std::chrono::seconds const call_dimmer_timeout{25};

    std::chrono::seconds const five_seconds{5};
    std::chrono::seconds const ten_seconds{10};
    std::chrono::seconds const fifteen_seconds{15};
    std::chrono::seconds const thirty_seconds{30};
    std::chrono::seconds const fourty_seconds{40};
    std::chrono::seconds const fifty_seconds{50};

    void expect_performance_boost_is_enabled()
    {
        using namespace testing;
        EXPECT_CALL(*performance_booster, enable_performance_boost_during_user_interaction());
    }

    void expect_performance_boost_is_disabled()
    {
        using namespace testing;
        EXPECT_CALL(*performance_booster, disable_performance_boost_during_user_interaction());
    }

    void expect_screen_is_turned_off()
    {
        using namespace testing;
        EXPECT_CALL(*screen_hardware, turn_off_backlight());
        EXPECT_CALL(*screen_hardware, allow_suspend());
        InSequence s;
        EXPECT_CALL(*compositor, stop());
        EXPECT_CALL(*display, configure(_));
    }

    void expect_screen_is_turned_on()
    {
        using namespace testing;
        EXPECT_CALL(*screen_hardware, disable_suspend());
        EXPECT_CALL(*screen_hardware, set_normal_backlight());
        InSequence s;
        EXPECT_CALL(*compositor, stop());
        EXPECT_CALL(*display, configure(_));
        EXPECT_CALL(*compositor, start());
    }

    void expect_screen_is_turned_dim()
    {
        using namespace testing;
        EXPECT_CALL(*screen_hardware, set_dim_backlight());
    }

    void expect_no_reconfiguration()
    {
        using namespace testing;
        EXPECT_CALL(*display, configure(_)).Times(0);
        EXPECT_CALL(*screen_hardware, set_dim_backlight()).Times(0);
    }

    void verify_proximity_enabled()
    {
        verify_and_clear_expectations();

        if (screen_hardware->current_proximity == MockScreenHardware::Proximity::far)
        {
            expect_screen_is_turned_off();
            cover_screen();
            verify_and_clear_expectations();

            expect_screen_is_turned_on();
            uncover_screen();
            verify_and_clear_expectations();
        }

        if (screen_hardware->current_proximity == MockScreenHardware::Proximity::near)
        {
            expect_screen_is_turned_on();
            uncover_screen();
            verify_and_clear_expectations();

            expect_screen_is_turned_off();
            cover_screen();
            verify_and_clear_expectations();
        }
    }

    void verify_proximity_disabled()
    {
        verify_and_clear_expectations();

        expect_no_reconfiguration();
        uncover_screen();
        cover_screen();
        uncover_screen();

        verify_and_clear_expectations();
    }

    void turn_screen_off()
    {
        using namespace testing;
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_off,
            PowerStateChangeReason::power_key);

        verify_and_clear_expectations();
    }

    void turn_screen_on()
    {
        using namespace testing;
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_on,
            PowerStateChangeReason::power_key);

        verify_and_clear_expectations();
    }

    void receive_notification()
    {
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_on,
            PowerStateChangeReason::notification);
        process_deferred_actions();
    }

    void receive_call()
    {
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_on,
            PowerStateChangeReason::snap_decision);
        process_deferred_actions();
    }

    void receive_call_done()
    {
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_on,
            PowerStateChangeReason::call_done);
        process_deferred_actions();
    }

    void cover_screen()
    {
        screen_hardware->set_proximity(MockScreenHardware::Proximity::near);
        process_deferred_actions();
    }

    void uncover_screen()
    {
        screen_hardware->set_proximity(MockScreenHardware::Proximity::far);
        process_deferred_actions();
    }

    void verify_and_clear_expectations()
    {
        using namespace testing;
        Mock::VerifyAndClearExpectations(screen_hardware.get());
        Mock::VerifyAndClearExpectations(display.get());
        Mock::VerifyAndClearExpectations(compositor.get());
    }

    std::shared_ptr<MockPerformanceBooster> performance_booster{
        std::make_shared<testing::NiceMock<MockPerformanceBooster>>()};
    std::shared_ptr<MockScreenHardware> screen_hardware{
        std::make_shared<testing::NiceMock<MockScreenHardware>>()};
    std::shared_ptr<MockCompositor> compositor{
        std::make_shared<testing::NiceMock<MockCompositor>>()};
    std::shared_ptr<ut::MockDisplay> display{
        std::make_shared<testing::NiceMock<ut::MockDisplay>>()};
    std::shared_ptr<MockTouchVisualizer> touch_visualizer{
        std::make_shared<testing::NiceMock<MockTouchVisualizer>>()};
    std::shared_ptr<AdvanceableTimer> timer{
        std::make_shared<AdvanceableTimer>()};

    usc::MirScreen mir_screen{
        performance_booster,
        screen_hardware,
        compositor,
        display,
        touch_visualizer,
        timer,
        timer,
        {power_off_timeout, dimmer_timeout},
        {notification_power_off_timeout, notification_dimmer_timeout},
        {call_power_off_timeout, call_dimmer_timeout}};
};

struct AParameterizedMirScreen : public AMirScreen, public ::testing::WithParamInterface<PowerStateChangeReason> {};
struct ImmediatePowerOnMirScreen : public AParameterizedMirScreen {};
struct DeferredPowerOnMirScreen : public AParameterizedMirScreen {};

}

TEST_P(ImmediatePowerOnMirScreen, enables_performance_boost_for_screen_on)
{
    turn_screen_off();
    expect_performance_boost_is_enabled();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on, GetParam());
}

TEST_P(DeferredPowerOnMirScreen, enables_performance_boost_for_screen_on_with_reason_proximity)
{
    turn_screen_off();
    expect_performance_boost_is_enabled();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on, GetParam());
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on, PowerStateChangeReason::proximity);
}

TEST_P(AParameterizedMirScreen, disables_performance_boost_for_screen_off)
{
    turn_screen_on();
    expect_performance_boost_is_disabled();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off, GetParam());
}

INSTANTIATE_TEST_CASE_P(
        AParameterizedMirScreen,
        AParameterizedMirScreen,
        ::testing::Values(PowerStateChangeReason::unknown, PowerStateChangeReason::inactivity, PowerStateChangeReason::power_key,
                          PowerStateChangeReason::proximity, PowerStateChangeReason::notification, PowerStateChangeReason::snap_decision,
                          PowerStateChangeReason::call_done));

INSTANTIATE_TEST_CASE_P(
        ImmediatePowerOnMirScreen,
        ImmediatePowerOnMirScreen,
        ::testing::Values(PowerStateChangeReason::unknown, PowerStateChangeReason::inactivity, PowerStateChangeReason::power_key));

INSTANTIATE_TEST_CASE_P(
        DeferredPowerOnMirScreen,
        DeferredPowerOnMirScreen,
        ::testing::Values(PowerStateChangeReason::notification, PowerStateChangeReason::snap_decision, PowerStateChangeReason::call_done));

TEST_F(AMirScreen, turns_screen_off_after_power_off_timeout)
{
    expect_screen_is_turned_off();

    timer->advance_by(power_off_timeout);
}

TEST_F(AMirScreen, dims_screen_after_dim_timeout)
{
    expect_screen_is_turned_dim();

    timer->advance_by(dimmer_timeout);
}

TEST_F(AMirScreen, keeps_display_on_while_already_on)
{
    using namespace testing;

    expect_no_reconfiguration();

    mir_screen.keep_display_on(true);
    timer->advance_by(power_off_timeout);
}

TEST_F(AMirScreen, turns_and_keeps_display_on_while_off)
{
    using namespace testing;

    turn_screen_off();

    expect_screen_is_turned_on();
    mir_screen.keep_display_on(true);

    verify_and_clear_expectations();

    expect_no_reconfiguration();
    timer->advance_by(power_off_timeout);
}

TEST_F(AMirScreen, does_not_turn_on_screen_temporarily_when_off)
{
    using namespace testing;

    turn_screen_off();

    expect_no_reconfiguration();
    mir_screen.keep_display_on_temporarily();
}

TEST_F(AMirScreen, keeps_screen_on_temporarily_when_already_on)
{
    using namespace testing;
    std::chrono::seconds const fourty_seconds{40};
    std::chrono::seconds const ten_seconds{10};

    expect_no_reconfiguration();

    // After keep_display_on_temporarily the timeouts should
    // be reset...
    timer->advance_by(fourty_seconds);
    mir_screen.keep_display_on_temporarily();

    // ... so 40 seconds after the reset (total 80 from start)
    // should trigger neither dim nor power off
    timer->advance_by(fourty_seconds);

    verify_and_clear_expectations();

    // Tens seconds more, 50 seconds from reset, the screen should dim
    expect_screen_is_turned_dim();
    timer->advance_by(ten_seconds);

    verify_and_clear_expectations();

    // Ten seconds second more, 60 seconds from reset, the screen should power off
    expect_screen_is_turned_off();
    timer->advance_by(ten_seconds);

    verify_and_clear_expectations();
}

TEST_F(AMirScreen, disabling_inactivity_timers_disables_dim_and_power_off)
{
    using namespace testing;

    expect_no_reconfiguration();

    mir_screen.enable_inactivity_timers(false);
    timer->advance_by(power_off_timeout);
}

TEST_F(AMirScreen, set_screen_power_mode_from_on_to_off)
{
    expect_screen_is_turned_off();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::power_key);
}

TEST_F(AMirScreen, set_screen_power_mode_from_off_to_on)
{
    turn_screen_off();

    expect_screen_is_turned_on();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);
}

TEST_F(AMirScreen, sets_hardware_brightness)
{
    int const brightness{10};

    EXPECT_CALL(*screen_hardware, set_brightness(brightness));

    mir_screen.set_brightness(brightness);
}

TEST_F(AMirScreen, sets_hardware_auto_brightness)
{
    bool const enable{true};

    EXPECT_CALL(*screen_hardware, enable_auto_brightness(enable));

    mir_screen.enable_auto_brightness(enable);
}

TEST_F(AMirScreen, forwards_touch_visualizations_requests_to_touch_visualizer)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(*touch_visualizer, enable());
    EXPECT_CALL(*touch_visualizer, disable());

    mir_screen.set_touch_visualization_enabled(true);
    mir_screen.set_touch_visualization_enabled(false);
}

TEST_F(AMirScreen, invokes_handler_when_power_state_changes)
{
    using namespace testing;

    auto handler_reason = PowerStateChangeReason::unknown;
    auto handler_mode = mir_power_mode_standby;

    auto handler =
        [&] (MirPowerMode mode, PowerStateChangeReason reason)
        {
            handler_mode = mode;
            handler_reason = reason;
        };

    mir_screen.register_power_state_change_handler(handler);

    auto const toggle_reason = PowerStateChangeReason::power_key;
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::power_key);

    EXPECT_THAT(handler_reason, Eq(toggle_reason));
    EXPECT_THAT(handler_mode, Eq(MirPowerMode::mir_power_mode_off));
}

TEST_F(AMirScreen, turns_screen_off_after_notification_timeout)
{
    turn_screen_off();

    expect_screen_is_turned_on();
    receive_notification();
    verify_and_clear_expectations();

    expect_screen_is_turned_off();
    timer->advance_by(notification_power_off_timeout);
}

TEST_F(AMirScreen, keep_display_on_temporarily_overrides_notification_timeout)
{
    turn_screen_off();

    expect_screen_is_turned_on();
    receive_notification();
    verify_and_clear_expectations();

    // At T=10 we request a temporary keep display on (e.g. user has touched
    // the screen)
    timer->advance_by(ten_seconds);
    mir_screen.keep_display_on_temporarily();

    // At T=20 nothing should happen since keep display on temporarily
    // has reset the timers (so the notification timeout of 15s is overriden).
    expect_no_reconfiguration();
    timer->advance_by(ten_seconds);
    verify_and_clear_expectations();

    // At T=70 (10 + 60) the screen should turn off due to the normal
    // inactivity timeout
    expect_screen_is_turned_off();
    timer->advance_by(fifty_seconds);
}

TEST_F(AMirScreen, notification_timeout_extends_active_timeout)
{
    // At T=0 we turn the screen on, and normal inactivity timeouts
    // are reset
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);

    // At T=50 we get a notification
    timer->advance_by(fifty_seconds);
    receive_notification();
    verify_and_clear_expectations();

    // At T=60 the screen should still be active because the notification
    // has extended the timeout.
    expect_no_reconfiguration();
    timer->advance_by(ten_seconds);
    verify_and_clear_expectations();

    // At T=65 (50 + 15) the screen should turn off due to the notification
    // inactivity timeout
    expect_screen_is_turned_off();
    timer->advance_by(five_seconds);
}

TEST_F(AMirScreen, notification_timeout_does_not_reduce_active_timeout)
{
    // At T=0 we turn the screen on, and normal inactivity timeouts
    // are reset
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);


    // At T=30 we get a notification
    timer->advance_by(thirty_seconds);
    receive_notification();
    verify_and_clear_expectations();

    // At T=45 the screen should still be active because the notification
    // has not reduced the active timeout.
    expect_no_reconfiguration();
    timer->advance_by(fifteen_seconds);
    verify_and_clear_expectations();

    // At T=50 the screen should be dimmed
    expect_screen_is_turned_dim();
    timer->advance_by(five_seconds);
    verify_and_clear_expectations();

    // At T=60 the screen should turn off due to the normal
    // inactivity timeout
    expect_screen_is_turned_off();
    timer->advance_by(ten_seconds);
}

TEST_F(AMirScreen, notification_timeout_can_extend_only_dimming)
{
    std::chrono::seconds const two_seconds{2};
    std::chrono::seconds const eight_seconds{8};

    // At T=0 we turn the screen on, and normal inactivity timeouts
    // are reset
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);

    // At T=40 we get a notification
    timer->advance_by(fourty_seconds);
    receive_notification();
    verify_and_clear_expectations();

    // At T=50 nothing should happen since the notification has
    // extended the dimming timeout
    expect_no_reconfiguration();
    timer->advance_by(ten_seconds);
    verify_and_clear_expectations();

    // At T=52 (40 + 12) screen should be dimmed due to the notification
    // dimming timeout
    expect_screen_is_turned_dim();
    timer->advance_by(two_seconds);
    verify_and_clear_expectations();

    // At T=60 the screen should turn off due to the normal
    // inactivity timeout
    expect_screen_is_turned_off();
    timer->advance_by(eight_seconds);
}

TEST_F(AMirScreen, proximity_requests_affect_screen_state)
{
    expect_screen_is_turned_off();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_screen_is_turned_on();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::proximity);
    verify_and_clear_expectations();
}

TEST_F(AMirScreen, proximity_requests_use_short_timeouts)
{
    // At T=0 we turn the screen on, and normal inactivity timeouts
    // are reset
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);

    // At T=30 we get a screen off request due to proximity
    timer->advance_by(thirty_seconds);
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::proximity);

    // At T=40 we get a screen on request due to proximity
    timer->advance_by(ten_seconds);
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::proximity);

    verify_and_clear_expectations();

    // At T=52 screen should be dimmed due to the short inactivity
    // dimming timeout
    expect_screen_is_turned_dim();
    timer->advance_by(notification_dimmer_timeout);
    verify_and_clear_expectations();

    // At T=55 the screen should turn off due to the short
    // inactivity timeout
    expect_screen_is_turned_off();
    timer->advance_by(std::chrono::seconds{3});
}

TEST_F(AMirScreen, does_not_turn_on_screen_when_notification_arrives_with_phone_covered)
{
    turn_screen_off();
    cover_screen();

    expect_no_reconfiguration();
    receive_notification();
}

TEST_F(AMirScreen, turns_screen_on_when_phone_is_uncovered_after_notification_arrives)
{
    turn_screen_off();
    cover_screen();

    expect_no_reconfiguration();
    receive_notification();
    verify_and_clear_expectations();

    expect_screen_is_turned_on();
    uncover_screen();
}

TEST_F(AMirScreen, cancels_proximity_handling_when_phone_is_turned_off_after_notification)
{
    turn_screen_off();
    cover_screen();

    receive_notification();
    timer->advance_by(notification_power_off_timeout);
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    uncover_screen();
    cover_screen();
}

TEST_F(AMirScreen, cancels_proximity_handling_when_screen_is_touched_after_notification)
{
    turn_screen_off();

    receive_notification();
    mir_screen.keep_display_on_temporarily();
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    cover_screen();
    uncover_screen();
}

TEST_F(AMirScreen, does_not_enable_proximity_handling_for_notification_when_screen_is_already_on)
{
    expect_no_reconfiguration();
    receive_notification();
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    cover_screen();
    uncover_screen();
}

TEST_F(AMirScreen, does_not_allow_proximity_to_turn_on_screen_not_turned_off_by_proximity)
{
    turn_screen_off();

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_on,
        PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_off,
        PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_on,
        PowerStateChangeReason::proximity);
}

TEST_F(AMirScreen, does_not_allow_proximity_to_turn_on_screen_not_turned_off_by_proximity_2)
{
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_off,
        PowerStateChangeReason::proximity);

    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_off,
        PowerStateChangeReason::power_key);

    verify_and_clear_expectations();

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_on,
        PowerStateChangeReason::proximity);
}

TEST_F(AMirScreen, proximity_can_affect_screen_after_keep_display_on)
{
    mir_screen.keep_display_on(true);

    expect_screen_is_turned_off();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_off,
        PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_screen_is_turned_on();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_on,
        PowerStateChangeReason::proximity);
}

TEST_F(AMirScreen, disables_active_timeout_when_setting_zero_inactivity_timeouts)
{
    std::chrono::hours const ten_hours{10};

    expect_no_reconfiguration();
    mir_screen.set_inactivity_timeouts(0, 0);
    timer->advance_by(ten_hours);
    verify_and_clear_expectations();
}

TEST_F(AMirScreen, notification_timeout_is_ignored_if_inactivity_timeouts_are_zero)
{
    std::chrono::hours const ten_hours{10};

    expect_no_reconfiguration();
    mir_screen.set_inactivity_timeouts(0, 0);
    receive_notification();
    timer->advance_by(ten_hours);
    verify_and_clear_expectations();
}

TEST_F(AMirScreen, notification_timeout_is_respected_when_screen_is_off_if_inactivity_timeouts_are_zero)
{
    mir_screen.set_inactivity_timeouts(0, 0);
    turn_screen_off();
    receive_notification();
    verify_and_clear_expectations();

    expect_screen_is_turned_off();
    timer->advance_by(notification_power_off_timeout);
    verify_and_clear_expectations();
}

TEST_F(AMirScreen, keep_display_on_temporarily_after_notification_keeps_display_on_forever_if_inactivity_timeouts_are_zero)
{
    std::chrono::hours const ten_hours{10};

    mir_screen.set_inactivity_timeouts(0, 0);
    turn_screen_off();
    
    receive_notification();
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    mir_screen.keep_display_on_temporarily();
    timer->advance_by(ten_hours);
    verify_and_clear_expectations();
}

TEST_F(AMirScreen, proximity_can_turn_on_screen_after_power_off_timeout_occurs_when_screen_is_off)
{
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);

    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::proximity);

    timer->advance_by(power_off_timeout);

    expect_screen_is_turned_on();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_screen_is_turned_off();

    timer->advance_by(power_off_timeout);
}

TEST_F(AMirScreen, proximity_can_turn_on_screen_after_power_off_timeout_occurs_when_screen_is_on)
{
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_screen_is_turned_off();
    timer->advance_by(power_off_timeout);
    verify_and_clear_expectations();

    expect_screen_is_turned_on();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_screen_is_turned_off();
    timer->advance_by(notification_power_off_timeout);
}

TEST_F(AMirScreen, proximity_cannot_turn_on_screen_if_power_key_turned_it_off)
{
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::power_key);

    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::power_key);

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(
        MirPowerMode::mir_power_mode_on,
        PowerStateChangeReason::proximity);
    verify_and_clear_expectations();

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_off,
                                     PowerStateChangeReason::proximity);

    verify_and_clear_expectations();

    expect_no_reconfiguration();
    mir_screen.set_screen_power_mode(MirPowerMode::mir_power_mode_on,
                                     PowerStateChangeReason::proximity);

}

TEST_F(AMirScreen, turns_screen_off_after_call_timeout)
{
    turn_screen_off();

    expect_screen_is_turned_on();
    receive_call();
    verify_and_clear_expectations();

    expect_screen_is_turned_off();
    timer->advance_by(call_power_off_timeout);
}

TEST_F(AMirScreen, keep_display_on_temporarily_overrides_call_timeout)
{
    turn_screen_off();

    expect_screen_is_turned_on();
    receive_call();
    verify_and_clear_expectations();

    // At T=20 we request a temporary keep display on (e.g. user has touched
    // the screen)
    timer->advance_by(ten_seconds);
    timer->advance_by(ten_seconds);
    mir_screen.keep_display_on_temporarily();

    // At T=30 nothing should happen since keep display on temporarily
    // has reset the timers (so the call timeout is overriden).
    expect_no_reconfiguration();
    timer->advance_by(ten_seconds);
    verify_and_clear_expectations();

    // At T=110 (50 + 60) the screen should turn off due to the normal
    // inactivity timeout
    expect_screen_is_turned_off();
    timer->advance_by(fifty_seconds);
}

TEST_F(AMirScreen, does_not_turn_on_screen_when_call_arrives_with_phone_covered)
{
    turn_screen_off();
    cover_screen();

    expect_no_reconfiguration();
    receive_call();
}

TEST_F(AMirScreen, enables_proximity_when_call_arrives)
{
    turn_screen_off();
    cover_screen();

    expect_no_reconfiguration();
    receive_call();
    verify_and_clear_expectations();

    verify_proximity_enabled();
}

TEST_F(AMirScreen, cancels_proximity_handling_when_screen_is_turned_off_by_call_timeout)
{
    turn_screen_off();
    cover_screen();

    receive_call();
    timer->advance_by(call_power_off_timeout);

    verify_proximity_disabled();
}

TEST_F(AMirScreen, cancels_proximity_handling_when_screen_is_touched_after_call)
{
    turn_screen_off();

    receive_call();
    mir_screen.keep_display_on_temporarily();

    verify_proximity_disabled();
}

TEST_F(AMirScreen, does_not_enable_proximity_handling_for_call_when_screen_is_already_on)
{
    expect_no_reconfiguration();
    receive_call();
    verify_and_clear_expectations();

    verify_proximity_disabled();
}

TEST_F(AMirScreen, retains_proximity_handling_when_call_done_arrives_when_screen_is_off_after_call)
{
    turn_screen_off();
    cover_screen();
    receive_call();
    receive_call_done();

    verify_proximity_enabled();
}

TEST_F(AMirScreen,
       turns_off_screen_and_proximity_soon_after_call_when_screen_not_covered)
{
    turn_screen_off();

    // Call
    receive_call();
    timer->advance_by(ten_seconds);
    receive_call_done();

    verify_and_clear_expectations();

    // After the notification timeout the screen should be off
    // and proximity should be disabled
    expect_screen_is_turned_off();
    timer->advance_by(notification_power_off_timeout);

    verify_proximity_disabled();
}

TEST_F(AMirScreen,
       turns_proximity_off_soon_after_call_when_screen_covered)
{
    turn_screen_off();
    cover_screen();

    expect_no_reconfiguration();

    // call
    receive_call();
    timer->advance_by(ten_seconds);
    receive_call_done();
    timer->advance_by(notification_power_off_timeout);

    verify_and_clear_expectations();

    verify_proximity_disabled();
}

TEST_F(AMirScreen,
       turns_off_screen_and_proximity_soon_after_call_when_screen_uncovered_after_call_is_received)
{
    turn_screen_off();
    cover_screen();

    // Uncover screen while in call
    receive_call();
    timer->advance_by(five_seconds);
    uncover_screen();
    timer->advance_by(five_seconds);
    receive_call_done();

    verify_and_clear_expectations();

    // After the notification timeout the screen should be off
    // and proximity should be disabled
    expect_screen_is_turned_off();
    timer->advance_by(notification_power_off_timeout);

    verify_proximity_disabled();
}

TEST_F(AMirScreen,
      keeps_screen_on_for_full_timeout_after_call_received_when_screen_was_turned_on_manually)
{
    // Turn screen on manually (i.e. with power key)
    turn_screen_off();
    turn_screen_on();

    expect_no_reconfiguration();

    // call
    receive_call();
    timer->advance_by(ten_seconds);
    receive_call_done();
    // Call done notification timeout should be ignored since
    // user had turned on the screen manually
    timer->advance_by(notification_power_off_timeout);

    verify_and_clear_expectations();

    verify_proximity_disabled();
}

TEST_F(AMirScreen,
       sets_normal_backlight_when_notification_arrives_while_screen_already_on)
{
    turn_screen_on();

    EXPECT_CALL(*screen_hardware, set_normal_backlight());
    receive_notification();
}

TEST_F(AMirScreen,
       sets_normal_backlight_when_call_arrives_while_screen_already_on)
{
    turn_screen_on();

    EXPECT_CALL(*screen_hardware, set_normal_backlight());
    receive_call();
}
