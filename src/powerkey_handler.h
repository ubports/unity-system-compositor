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

#ifndef POWERKEY_HANDLER_H_
#define POWERKEY_HANDLER_H_

#include "mir/input/event_filter.h"

#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace mir
{
class DefaultServerConfiguration;
namespace time
{
class Alarm;
class Timer;
}
}

class ScreenStateHandler;

class PowerKeyHandler : public mir::input::EventFilter
{
public:
    PowerKeyHandler(mir::time::Timer& timer,
                    std::chrono::milliseconds power_key_ignore_timeout,
                    std::chrono::milliseconds shutdown_timeout,
                    ScreenStateHandler& screen_state_handler);

    ~PowerKeyHandler();

    bool handle(MirEvent const& event) override;

private:
    void power_key_up();
    void power_key_down();
    void shutdown_alarm_notification();
    void long_press_notification();

    std::mutex guard;
    std::atomic<bool> long_press_detected;

    ScreenStateHandler* screen_state_handler;

    std::chrono::milliseconds power_key_ignore_timeout;
    std::chrono::milliseconds shutdown_timeout;

    std::unique_ptr<mir::time::Alarm> shutdown_alarm;
    std::unique_ptr<mir::time::Alarm> long_press_alarm;

};

#endif
