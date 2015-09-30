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

#ifndef USC_MIR_SCREEN_H_
#define USC_MIR_SCREEN_H_

#include "screen.h"
#include <mir/time/types.h>

#include <chrono>
#include <memory>
#include <mutex>

enum class PowerStateChangeReason;

namespace mir
{
namespace compositor { class Compositor; }
namespace graphics {class Display;}
namespace input { class TouchVisualizer; }
namespace time { class AlarmFactory; class Alarm; }
}

namespace usc
{
class Server;
class ScreenHardware;
class Clock;

class MirScreen: public Screen
{
public:
    struct Timeouts
    {
        std::chrono::milliseconds power_off_timeout;
        std::chrono::milliseconds dimming_timeout;
    };

    MirScreen(std::shared_ptr<usc::ScreenHardware> const& screen_hardware,
              std::shared_ptr<mir::compositor::Compositor> const& compositor,
              std::shared_ptr<mir::graphics::Display> const& display,
              std::shared_ptr<mir::input::TouchVisualizer> const& touch_visualizer,
              std::shared_ptr<mir::time::AlarmFactory> const& alarm_factory,
              std::shared_ptr<usc::Clock> const& clock,
              Timeouts inactivity_timeouts,
              Timeouts notification_timeouts,
              Timeouts call_timeouts);
    ~MirScreen();

    void enable_inactivity_timers(bool enable) override;
    void keep_display_on_temporarily() override;

    MirPowerMode get_screen_power_mode() override;
    void set_screen_power_mode(MirPowerMode mode, PowerStateChangeReason reason) override;
    void keep_display_on(bool on) override;
    void set_brightness(int brightness) override;
    void enable_auto_brightness(bool enable) override;
    void set_inactivity_timeouts(int power_off_timeout, int dimmer_timeout) override;

    void set_touch_visualization_enabled(bool enabled) override;
    void register_power_state_change_handler(
            PowerStateChangeHandler const& power_state_change_handler) override;

protected:
    // These are protected virtual because we need to override them in tests
    virtual void power_off_alarm_notification();
    virtual void dimmer_alarm_notification();

private:
    enum class ForceResetTimers { no, yes };
    class LockableCallback;
    class PowerOffLockableCallback;
    class DimmerLockableCallback;

    void set_screen_power_mode_l(MirPowerMode mode, PowerStateChangeReason reason);
    void configure_display_l(MirPowerMode mode, PowerStateChangeReason reason);

    void cancel_timers_l(PowerStateChangeReason reason);
    void reset_timers_l(PowerStateChangeReason reason);
    void reset_timers_ignoring_power_mode_l(PowerStateChangeReason reason, ForceResetTimers force);
    void enable_inactivity_timers_l(bool flag);
    Timeouts timeouts_for(PowerStateChangeReason reason);
    bool is_screen_change_allowed(MirPowerMode mode, PowerStateChangeReason reason);

    void long_press_alarm_notification();

    std::shared_ptr<usc::ScreenHardware> const screen_hardware;
    std::shared_ptr<mir::compositor::Compositor> const compositor;
    std::shared_ptr<mir::graphics::Display> const display;
    std::shared_ptr<mir::input::TouchVisualizer> const touch_visualizer;
    std::shared_ptr<mir::time::AlarmFactory> const alarm_factory;
    std::shared_ptr<usc::Clock> const clock;
    std::unique_ptr<mir::time::Alarm> const power_off_alarm;
    std::unique_ptr<mir::time::Alarm> const dimmer_alarm;

    std::mutex guard;
    Timeouts inactivity_timeouts;
    Timeouts notification_timeouts;
    Timeouts snap_decision_timeouts;
    mir::time::Timestamp next_power_off{};
    mir::time::Timestamp next_dimming{};
    MirPowerMode current_power_mode;
    bool restart_timers;
    PowerStateChangeHandler power_state_change_handler;
    bool allow_proximity_to_turn_on_screen;
    bool turned_on_by_user;
};

}

#endif
