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

#include "mir_screen.h"

#include <mir/main_loop.h>
#include <mir/time/alarm_factory.h>
#include <mir/compositor/compositor.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>
#include <mir/input/touch_visualizer.h>

#include <cstdio>
#include "screen_hardware.h"
#include "power_state_change_reason.h"
#include "server.h"

namespace mi = mir::input;
namespace mg = mir::graphics;

usc::MirScreen::MirScreen(
    std::shared_ptr<usc::ScreenHardware> const& screen_hardware,
    std::shared_ptr<mir::compositor::Compositor> const& compositor,
    std::shared_ptr<mir::graphics::Display> const& display,
    std::shared_ptr<mir::input::TouchVisualizer> const& touch_visualizer,
    std::shared_ptr<mir::time::AlarmFactory> const& alarm_factory,
    std::chrono::milliseconds power_off_timeout,
    std::chrono::milliseconds dimmer_timeout)
    : screen_hardware{screen_hardware},
      compositor{compositor},
      display{display},
      touch_visualizer{touch_visualizer},
      alarm_factory{alarm_factory},
      power_off_alarm{alarm_factory->create_alarm(
              std::bind(&usc::MirScreen::power_off_alarm_notification, this))},
      dimmer_alarm{alarm_factory->create_alarm(
              std::bind(&usc::MirScreen::dimmer_alarm_notification, this))},
      power_off_timeout{power_off_timeout},
      dimming_timeout{dimmer_timeout},
      current_power_mode{MirPowerMode::mir_power_mode_on},
      restart_timers{true},
      power_state_change_handler{[](MirPowerMode,PowerStateChangeReason){}}
{
    /*
     * Make sure the compositor is running as certain conditions can
     * cause Mir to tear down the compositor threads before we get
     * to this point. See bug #1410381.
     */
    compositor->start();
    reset_timers_l();
}

usc::MirScreen::~MirScreen() = default;

void usc::MirScreen::keep_display_on_temporarily()
{
    std::lock_guard<std::mutex> lock{guard};
    reset_timers_l();
    if (current_power_mode == MirPowerMode::mir_power_mode_on)
        screen_hardware->set_normal_backlight();
}

void usc::MirScreen::enable_inactivity_timers(bool enable)
{
    std::lock_guard<std::mutex> lock{guard};
    enable_inactivity_timers_l(enable);
}

void usc::MirScreen::toggle_screen_power_mode(PowerStateChangeReason reason)
{
    std::lock_guard<std::mutex> lock{guard};
    MirPowerMode new_mode = (current_power_mode == MirPowerMode::mir_power_mode_on) ?
            MirPowerMode::mir_power_mode_off : MirPowerMode::mir_power_mode_on;

    set_screen_power_mode_l(new_mode, reason);
}

void usc::MirScreen::set_screen_power_mode(MirPowerMode mode, PowerStateChangeReason reason)
{
    std::lock_guard<std::mutex> lock{guard};
    set_screen_power_mode_l(mode, reason);
}

void usc::MirScreen::keep_display_on(bool on)
{
    std::lock_guard<std::mutex> lock{guard};
    restart_timers = !on;
    enable_inactivity_timers_l(!on);

    if (on)
        set_screen_power_mode_l(MirPowerMode::mir_power_mode_on, PowerStateChangeReason::unknown);
}

void usc::MirScreen::set_brightness(int brightness)
{
    std::lock_guard<std::mutex> lock{guard};
    screen_hardware->set_brightness(brightness);
}

void usc::MirScreen::enable_auto_brightness(bool enable)
{
    std::lock_guard<std::mutex> lock{guard};
    screen_hardware->enable_auto_brightness(enable);
}

void usc::MirScreen::set_inactivity_timeouts(int raw_poweroff_timeout, int raw_dimmer_timeout)
{
    std::lock_guard<std::mutex> lock{guard};

    std::chrono::seconds the_power_off_timeout{raw_poweroff_timeout};
    std::chrono::seconds the_dimming_timeout{raw_dimmer_timeout};

    if (raw_poweroff_timeout >= 0)
        power_off_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(the_power_off_timeout);

    if (raw_dimmer_timeout >= 0)
        dimming_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(the_dimming_timeout);

    cancel_timers_l();
    reset_timers_l();
}

void usc::MirScreen::set_screen_power_mode_l(MirPowerMode mode, PowerStateChangeReason reason)
{
    if (mode == MirPowerMode::mir_power_mode_on)
    {
        /* The screen may be dim, but on - make sure to reset backlight */
        if (current_power_mode == MirPowerMode::mir_power_mode_on)
            screen_hardware->set_normal_backlight();
        configure_display_l(mode, reason);
        reset_timers_l();
    }
    else
    {
        cancel_timers_l();
        configure_display_l(mode, reason);
    }
}

void usc::MirScreen::configure_display_l(MirPowerMode mode, PowerStateChangeReason reason)
{
    if (current_power_mode == mode)
        return;

    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();

    displayConfig->for_each_output(
        [&](const mg::UserDisplayConfigurationOutput displayConfigOutput) {
            displayConfigOutput.power_mode = mode;
        }
    );

    compositor->stop();

    bool const power_on = mode == MirPowerMode::mir_power_mode_on;
    if (power_on)
    {
        //Some devices do not turn screen on properly from suspend mode
        screen_hardware->disable_suspend();
    }
    else
    {
        screen_hardware->turn_off_backlight();
    }

    display->configure(*displayConfig.get());

    if (power_on)
    {
        compositor->start();
        screen_hardware->set_normal_backlight();
    }

    current_power_mode = mode;

    // TODO: Don't call this under lock
    power_state_change_handler(mode, reason);

    if (!power_on)
        screen_hardware->allow_suspend();
}

void usc::MirScreen::cancel_timers_l()
{
    power_off_alarm->cancel();
    dimmer_alarm->cancel();
}

void usc::MirScreen::reset_timers_l()
{
    if (restart_timers && current_power_mode != MirPowerMode::mir_power_mode_off)
    {
        if (power_off_timeout.count() > 0)
            power_off_alarm->reschedule_in(power_off_timeout);

        if (dimming_timeout.count() > 0)
            dimmer_alarm->reschedule_in(dimming_timeout);
    }
}

void usc::MirScreen::enable_inactivity_timers_l(bool enable)
{
    if (enable)
        reset_timers_l();
    else
        cancel_timers_l();
}

void usc::MirScreen::power_off_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    configure_display_l(MirPowerMode::mir_power_mode_off, PowerStateChangeReason::inactivity);
}

void usc::MirScreen::dimmer_alarm_notification()
{
    std::lock_guard<std::mutex> lock{guard};
    screen_hardware->set_dim_backlight();
}

void usc::MirScreen::set_touch_visualization_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock{guard};

    if (enabled)
        touch_visualizer->enable();
    else
        touch_visualizer->disable();
}

void usc::MirScreen::register_power_state_change_handler(
    PowerStateChangeHandler const& handler)
{
    std::lock_guard<std::mutex> lock{guard};

    power_state_change_handler = handler;
}
