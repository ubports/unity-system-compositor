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

#include <mir/input/event_filter.h>
#include <mir/time/types.h>

#include <memory>
#include <mutex>
#include <chrono>

namespace usc
{
class PowerButtonEventSink;
class UserActivityEventSink;
class Clock;

class ScreenEventHandler : public mir::input::EventFilter
{
public:
    ScreenEventHandler(
        std::shared_ptr<PowerButtonEventSink> const& power_button_event_sink,
        std::shared_ptr<UserActivityEventSink> const& user_activity_event_sink,
        std::shared_ptr<Clock> const& clock);

    bool handle(MirEvent const& event) override;

private:
    void notify_activity_changing_power_state();
    void notify_activity_extending_power_state();

    std::shared_ptr<PowerButtonEventSink> const power_button_event_sink;
    std::shared_ptr<UserActivityEventSink> const user_activity_event_sink;
    std::shared_ptr<Clock> const clock;
    std::chrono::milliseconds const event_period{500};

    std::mutex event_mutex;
    mir::time::Timestamp last_activity_changing_power_state_event_time;
    mir::time::Timestamp last_activity_extending_power_state_event_time;
};

}

#endif
