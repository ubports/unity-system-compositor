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

#ifndef USC_SCREEN_EVENT_HANDLER_H_
#define USC_SCREEN_EVENT_HANDLER_H_

#include "mir/input/event_filter.h"

#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>

namespace mir
{
namespace time
{
class Alarm;
class AlarmFactory;
}
}

namespace usc
{
class Screen;

class ScreenEventHandler : public mir::input::EventFilter
{
public:
    ScreenEventHandler(
        std::shared_ptr<Screen> const& screen,
        std::shared_ptr<mir::time::AlarmFactory> const& alarm_factory,
        std::chrono::milliseconds power_key_ignore_timeout,
        std::chrono::milliseconds shutdown_timeout,
        std::function<void()> const& shutdown);

    ~ScreenEventHandler();

    bool handle(MirEvent const& event) override;

private:
    void power_key_up();
    void power_key_down();
    void shutdown_alarm_notification();
    void long_press_notification();

    std::mutex guard;
    std::shared_ptr<Screen> const screen;
    std::shared_ptr<mir::time::AlarmFactory> const alarm_factory;
    std::chrono::milliseconds const power_key_ignore_timeout;
    std::chrono::milliseconds const shutdown_timeout;
    std::function<void()> const shutdown;

    std::atomic<bool> long_press_detected;
    std::unique_ptr<mir::time::Alarm> shutdown_alarm;
    std::unique_ptr<mir::time::Alarm> long_press_alarm;
};

}

#endif
