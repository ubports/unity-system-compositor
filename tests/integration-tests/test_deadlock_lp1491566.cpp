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

#include "src/server.h"
#include "src/mir_screen.h"
#include "src/screen_hardware.h"
#include "src/power_state_change_reason.h"
#include "spin_wait.h"

#include <mir/compositor/compositor.h>
#include <mir/main_loop.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>
#include <mir/graphics/gl_context.h>
#include <mir/input/touch_visualizer.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <future>

namespace mg = mir::graphics;

namespace
{

struct NullCompositor : mir::compositor::Compositor
{
    void start() override {}
    void stop() override {}
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

struct StubDisplay : mg::Display
{
    void for_each_display_sync_group(std::function<void(mg::DisplaySyncGroup&)> const& f) override
    {
    }

    std::unique_ptr<mg::DisplayConfiguration> configuration() const override
    { 
        return std::unique_ptr<mg::DisplayConfiguration>{new StubDisplayConfiguration{}};
    }

    void configure(mg::DisplayConfiguration const&) override {}

    void register_configuration_change_handler(
        mg::EventHandlerRegister& ,
        mg::DisplayConfigurationChangeHandler const& ) override
    {
    }

    void register_pause_resume_handlers(
        mg::EventHandlerRegister&,
        mg::DisplayPauseHandler const&,
        mg::DisplayResumeHandler const&) override
    {
    }

    void pause() override {}
    void resume() override {}

    std::shared_ptr<mg::Cursor> create_hardware_cursor(
        std::shared_ptr<mg::CursorImage> const&) override
    {
        return{};
    }

    std::unique_ptr<mg::GLContext> create_gl_context() override
    { 
        return std::unique_ptr<mg::GLContext>{};
    }
};

struct StubScreenHardware : usc::ScreenHardware
{
    void set_dim_backlight() override {}
    void set_normal_backlight() override {}
    void turn_off_backlight() override {}

    void change_backlight_values(int dim_brightness, int normal_brightness) override {}

    void allow_suspend() override {}
    void disable_suspend() override {}

    void enable_auto_brightness(bool) override {}
    bool auto_brightness_supported() override { return true; }
    void set_brightness(int) override {}
    int min_brightness() override { return 100; }
    int max_brightness() override { return 0; }

    void enable_proximity(bool) override { }
};

struct NullTouchVisualizer : mir::input::TouchVisualizer
{
    void enable() override {}
    void disable() override {}
    void visualize_touches(std::vector<Spot> const& touches) override {}
};

class TestMirScreen : public usc::MirScreen
{
public:
    using usc::MirScreen::MirScreen;

    void dimmer_alarm_notification() override
    {
        before_dimmer_alarm_func();
        usc::MirScreen::dimmer_alarm_notification();
    }

    void power_off_alarm_notification() override
    {
        before_power_off_alarm_func();
        usc::MirScreen::power_off_alarm_notification();
    }

    std::function<void()> before_dimmer_alarm_func = []{};
    std::function<void()> before_power_off_alarm_func = []{};
};

struct DeadlockLP1491566 : public testing::Test
{
    DeadlockLP1491566()
    {
        main_loop_thread = std::thread{[this] { main_loop->run(); }};
    }

    ~DeadlockLP1491566()
    {
        main_loop->stop();
        main_loop_thread.join();
    }

    std::future<void> async_set_screen_power_mode()
    {
        return std::async(
            std::launch::async,
            [this]
            {
                mir_screen.set_screen_power_mode(
                    MirPowerMode::mir_power_mode_on,
                    PowerStateChangeReason::power_key);
            });
    }

    void schedule_inactivity_handlers()
    {
        // We set the screen power mode to on, which will
        // schedule inactivity handlers
        mir_screen.set_screen_power_mode(
            MirPowerMode::mir_power_mode_on,
            PowerStateChangeReason::power_key);
    }

    void wait_for_async_operation(std::future<void>& future)
    {
        if (!usc::test::spin_wait_for_condition_or_timeout(
                [&future] { return future.valid(); },
                std::chrono::seconds{3}))
        {
            throw std::runtime_error{"Future is not valid!"};
        }

        if (future.wait_for(std::chrono::seconds{3}) != std::future_status::ready)
        {
            std::cerr << "Deadlock detected. Aborting." << std::endl;
            abort();
        }
    }

    char *argv[1] = {nullptr};
    usc::Server server{0, argv};
    std::shared_ptr<mir::MainLoop> const main_loop{server.the_main_loop()};
    std::chrono::milliseconds const power_off_timeout{200};
    std::chrono::milliseconds const dimmer_timeout{100};

    TestMirScreen mir_screen{
        std::make_shared<StubScreenHardware>(),
        std::make_shared<NullCompositor>(),
        std::make_shared<StubDisplay>(),
        std::make_shared<NullTouchVisualizer>(),
        main_loop,
        server.the_clock(),
        {power_off_timeout, dimmer_timeout},
        {power_off_timeout, dimmer_timeout},
        {power_off_timeout, dimmer_timeout}};

    std::thread main_loop_thread;
};

}

// The following tests reproduce the situation we are seeing in bug 1491566,
// where a set_screen_power_mode request tries to run concurrently with either
// the dimmer or power-off timeout handlers. The deadlock is caused by two
// threads trying to acquire the internal alarm lock and the MirScreen lock in
// opposite orders.
//
// To reproduce the race in these tests, we need to run the
// set_screen_power_mode request just before the timeout handlers are invoked,
// while the thread invoking the timeout handlers holds the internal alarm
// lock, but before it has acquired the MirScreen lock. The thread calling
// set_screen_power_mode will then first acquire the MirScreen lock and then
// try to acquire the internal alarm clock and block. The timeout handler
// thread will eventually resume and try to get the MirScreen lock, leading to
// a deadlock.

TEST_F(DeadlockLP1491566, between_dimmer_handler_and_screen_power_mode_request_is_averted)
{
    std::future<void> future;

    mir_screen.before_dimmer_alarm_func = 
        [this, &future]
        {
            future = async_set_screen_power_mode();
            // Wait a bit for async operation to get processed
            std::this_thread::sleep_for(
                std::chrono::milliseconds{500});
        };

    schedule_inactivity_handlers();

    wait_for_async_operation(future);
}

TEST_F(DeadlockLP1491566, between_power_off_handler_and_screen_power_mode_request_is_averted)
{
    std::future<void> future;

    mir_screen.before_power_off_alarm_func = 
        [this, &future]
        {
            future = async_set_screen_power_mode();
            // Wait a bit for async operation to get processed
            std::this_thread::sleep_for(
                std::chrono::milliseconds{500});
        };

    schedule_inactivity_handlers();

    wait_for_async_operation(future);
}
