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

#include <memory>
#include <mutex>

class OneshotTimer;
class DBusScreen;
class PowerdMediator;

namespace mir
{
    class DefaultServerConfiguration;
}

class ScreenStateHandler: public mir::input::EventFilter
{
public:
    ScreenStateHandler(std::shared_ptr<mir::DefaultServerConfiguration> const& config,
                       int power_off_timeout,
                       int dimmer_timeout);
    ~ScreenStateHandler();

    //from EventFilter
    bool handle(MirEvent const& event) override;

    //timeout values in seconds
    void set_timeouts(int power_off_delay, int dimming_delay);

private:
    void toggle_screen_power_mode();
    void set_screen_power_mode(MirPowerMode mode);
    void set_display_power_mode(MirPowerMode mode);


    void display_on();
    void display_off();

    void power_off_timer_notification();
    void dimmer_timer_notification();

    void cancel_timers();
    void reset_timers();

    std::unique_ptr<OneshotTimer> power_off_timer;
    std::unique_ptr<OneshotTimer> dimmer_timer;
    MirPowerMode current_power_mode;

    int power_off_delay;
    int dimming_delay;
    bool auto_brightness;

    std::mutex power_mode_mutex;
    std::unique_ptr<DBusScreen> dbus_screen;
    std::unique_ptr<PowerdMediator> powerd_mediator;
    std::shared_ptr<mir::DefaultServerConfiguration> config;
};

#endif
