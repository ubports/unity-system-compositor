/*
 * Copyright © 2014 Canonical Ltd.
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

#ifndef SCREEN_STATE_HANDLER_
#define SCREEN_STATE_HANDLER_

#include "mir/input/event_filter.h"
#include "dbus_screen_observer.h"

#include <chrono>
#include <memory>
#include <mutex>

class DBusScreen;
class PowerdMediator;
enum class PowerStateChangeReason;

namespace mir
{
class DefaultServerConfiguration;
namespace time
{
class Alarm;
class Timer;
}
}

class ScreenStateHandler: public mir::input::EventFilter, public DBusScreenObserver
{
public:
    ScreenStateHandler(std::shared_ptr<mir::DefaultServerConfiguration> const& config,
                       std::chrono::milliseconds power_off_timeout,
                       std::chrono::milliseconds dimmer_timeout);
    virtual ~ScreenStateHandler();

    //from EventFilter
    bool handle(MirEvent const& event) override;

    void set_timeouts(std::chrono::milliseconds power_off_timeout,
                      std::chrono::milliseconds dimmer_timeout);

    void enable_inactivity_timers(bool enable);
    void toggle_screen_power_mode();

    void set_screen_power_mode(MirPowerMode mode, PowerStateChangeReason reason) override;
    void keep_display_on(bool on) override;
    void set_brightness(int brightness) override;
    void enable_auto_brightness(bool enable) override;

private:
    void set_screen_power_mode_l(MirPowerMode mode, PowerStateChangeReason reason);
    void configure_display_l(MirPowerMode mode, PowerStateChangeReason reason);

    void cancel_timers_l();
    void reset_timers_l();
    void enable_inactivity_timers_l(bool flag);

    void power_off_alarm_notification();
    void dimmer_alarm_notification();
    void long_press_alarm_notification();

    std::mutex guard;

    MirPowerMode current_power_mode;
    bool restart_timers;

    std::chrono::milliseconds power_off_timeout;
    std::chrono::milliseconds dimming_timeout;

    std::unique_ptr<PowerdMediator> powerd_mediator;
    std::shared_ptr<mir::DefaultServerConfiguration> config;

    std::unique_ptr<mir::time::Alarm> power_off_alarm;
    std::unique_ptr<mir::time::Alarm> dimmer_alarm;

    std::unique_ptr<DBusScreen> dbus_screen;
};

#endif
