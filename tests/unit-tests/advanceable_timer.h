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

#ifndef USC_TESTS_ADVANCEABLE_TIMER_H_
#define USC_TESTS_ADVANCEABLE_TIMER_H_

#include <mir/time/alarm_factory.h>
#include "src/clock.h"

#include <vector>
#include <mutex>
#include <memory>

namespace detail
{
class AdvanceableAlarm;
}

class AdvanceableTimer : public mir::time::AlarmFactory, public usc::Clock
{
public:
    // mir::time::AlarmFactory
    std::unique_ptr<mir::time::Alarm> create_alarm(
        std::function<void()> const& callback) override;
    std::unique_ptr<mir::time::Alarm> create_alarm(
        std::shared_ptr<mir::LockableCallback> const& callback) override;

    // usc::Clock
    mir::time::Timestamp now() const override;

    void advance_by(std::chrono::milliseconds advance);

private:
    void register_alarm(std::shared_ptr<detail::AdvanceableAlarm> const& alarm);
    void trigger_alarms();

    mutable std::mutex now_mutex;
    mir::time::Timestamp now_{};
    std::mutex alarms_mutex;
    std::vector<std::weak_ptr<detail::AdvanceableAlarm>> alarms;
};

#endif
