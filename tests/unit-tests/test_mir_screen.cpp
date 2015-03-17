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
#include "src/screen_hardware.h"
#include "src/power_state_change_reason.h"
#include "advanceable_timer.h"

#include <mir/compositor/compositor.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>
#include <mir/graphics/gl_context.h>
#include <mir/input/touch_visualizer.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>

namespace mg = mir::graphics;

namespace
{

struct MockCompositor : mir::compositor::Compositor
{
    MOCK_METHOD0(start, void());
    MOCK_METHOD0(stop, void());
};

struct StubDisplayConfiguration : mg::DisplayConfiguration
{
    StubDisplayConfiguration()
    {
        conf_output.power_mode = MirPowerMode::mir_power_mode_on;
    }

    void for_each_card(std::function<void(mg::DisplayConfigurationCard const&)> f) const override
    {
    }

    void for_each_output(std::function<void(mg::DisplayConfigurationOutput const&)> f) const override
    {
        f(conf_output);
    }

    void for_each_output(std::function<void(mg::UserDisplayConfigurationOutput&)> f)
    {
        mg::UserDisplayConfigurationOutput user{conf_output};
        f(user);
    }

    mg::DisplayConfigurationOutput conf_output;
};

struct MockDisplay : mg::Display
{
    void for_each_display_sync_group(std::function<void(mg::DisplaySyncGroup&)> const& f) override
    {
    }

    std::unique_ptr<mg::DisplayConfiguration> configuration() const override
    { return std::unique_ptr<mg::DisplayConfiguration>{new StubDisplayConfiguration{}}; }

    MOCK_METHOD1(configure, void(mg::DisplayConfiguration const& conf));

    void register_configuration_change_handler(
    mg::EventHandlerRegister& ,
    mg::DisplayConfigurationChangeHandler const& ) override {};

    void register_pause_resume_handlers(
        mg::EventHandlerRegister&,
        mg::DisplayPauseHandler const&,
        mg::DisplayResumeHandler const&) override
    {
    }

    void pause() override {};

    void resume() override {};

    std::shared_ptr<mg::Cursor> create_hardware_cursor(std::shared_ptr<mg::CursorImage> const&) override {return{};};

    std::unique_ptr<mg::GLContext> create_gl_context() override
    { return std::unique_ptr<mg::GLContext>{};};
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
    std::chrono::milliseconds power_off_timeout{5000};
    std::chrono::milliseconds dimmer_timeout{4000};

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

    void turn_screen_off()
    {
        using namespace testing;
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_off,
            PowerStateChangeReason::power_key);

        verify_and_clear_expectations();
    }

    void verify_and_clear_expectations()
    {
        using namespace testing;
        Mock::VerifyAndClearExpectations(screen_hardware.get());
        Mock::VerifyAndClearExpectations(display.get());
        Mock::VerifyAndClearExpectations(compositor.get());
    }

    std::shared_ptr<MockScreenHardware> screen_hardware{
        std::make_shared<testing::NiceMock<MockScreenHardware>>()};
    std::shared_ptr<MockCompositor> compositor{
        std::make_shared<testing::NiceMock<MockCompositor>>()};
    std::shared_ptr<MockDisplay> display{
        std::make_shared<testing::NiceMock<MockDisplay>>()};
    std::shared_ptr<MockTouchVisualizer> touch_visualizer{
        std::make_shared<testing::NiceMock<MockTouchVisualizer>>()};
    std::shared_ptr<AdvanceableTimer> timer{
        std::make_shared<AdvanceableTimer>()};

    usc::MirScreen mir_screen{
        screen_hardware,
        compositor,
        display,
        touch_visualizer,
        timer,
        power_off_timeout,
        dimmer_timeout};
};

}

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
    std::chrono::seconds const three_seconds{3};
    std::chrono::seconds const one_second{1};

    expect_no_reconfiguration();

    // After keep_display_on_temporarily the timeouts should
    // be reset...
    timer->advance_by(three_seconds);
    mir_screen.keep_display_on_temporarily();

    // ... so 3 seconds after the reset (total 6 from start)
    // should trigger neither dim nor power off
    timer->advance_by(three_seconds);

    verify_and_clear_expectations();

    // A second more, 4 seconds from reset, the screen should dim
    expect_screen_is_turned_dim();
    timer->advance_by(one_second);

    verify_and_clear_expectations();

    // A second more, 5 seconds from reset, the screen should power off
    expect_screen_is_turned_off();
    timer->advance_by(one_second);

    verify_and_clear_expectations();
}

TEST_F(AMirScreen, disabling_inactivity_timers_disables_dim_and_power_off)
{
    using namespace testing;

    expect_no_reconfiguration();

    mir_screen.enable_inactivity_timers(false);
    timer->advance_by(power_off_timeout);
}

TEST_F(AMirScreen, toggle_screen_power_mode_from_on_to_off)
{
    expect_screen_is_turned_off();
    mir_screen.toggle_screen_power_mode(PowerStateChangeReason::power_key);
}

TEST_F(AMirScreen, toggle_screen_power_mode_from_off_to_on)
{
    turn_screen_off();

    expect_screen_is_turned_on();
    mir_screen.toggle_screen_power_mode(PowerStateChangeReason::power_key);
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
    mir_screen.toggle_screen_power_mode(PowerStateChangeReason::power_key);

    EXPECT_THAT(handler_reason, Eq(toggle_reason));
    EXPECT_THAT(handler_mode, Eq(MirPowerMode::mir_power_mode_off));
}
