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

#include <chrono>
#include <memory>
#include <mutex>

class DBusScreen;
class DBusPowerKey;
class PowerdMediator;

namespace mir
{
class DefaultServerConfiguration;
namespace time
{
class Alarm;
}
}

class ScreenStateHandler: public mir::input::EventFilter
{
public:
    ScreenStateHandler(std::shared_ptr<mir::DefaultServerConfiguration> const& config,
                       std::chrono::milliseconds power_off_timeout,
                       std::chrono::milliseconds dimmer_timeout,
                       std::chrono::milliseconds power_key_held_timeout);
    ~ScreenStateHandler();

    //from EventFilter
    bool handle(MirEvent const& event) override;

    void set_timeouts(std::chrono::milliseconds power_off_timeout,
                      std::chrono::milliseconds dimmer_timeout);

private:
    void set_screen_power_mode_l(MirPowerMode mode);
    void toggle_screen_power_mode_l();
    void configure_display_l(MirPowerMode mode);

    void cancel_timers_l();
    void reset_timers_l();

    void power_off_alarm_notification();
    void dimmer_alarm_notification();
    void long_press_alarm_notification();
    void power_key_up();
    void power_key_down();

    std::mutex guard;

    MirPowerMode current_power_mode;
    bool long_press_detected;

    std::chrono::milliseconds power_off_timeout;
    std::chrono::milliseconds dimming_timeout;
    std::chrono::milliseconds power_key_long_press_timeout;

    std::unique_ptr<PowerdMediator> powerd_mediator;
    std::shared_ptr<mir::DefaultServerConfiguration> config;

    std::unique_ptr<mir::time::Alarm> power_off_alarm;
    std::unique_ptr<mir::time::Alarm> dimmer_alarm;
    std::unique_ptr<mir::time::Alarm> long_press_alarm;

    std::unique_ptr<DBusScreen> dbus_screen;
    std::unique_ptr<DBusPowerKey> dbus_powerkey;
};

#endif
