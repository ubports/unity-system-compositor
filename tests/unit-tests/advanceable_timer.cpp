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

#include "advanceable_timer.h"
#include <mir/lockable_callback.h>
#include <algorithm>

namespace
{

struct AlarmWrapper : mir::time::Alarm
{
    AlarmWrapper(
        std::shared_ptr<mir::time::Alarm> const& alarm)
        : wrapped_alarm{alarm}
    {
    }

    bool cancel() override
    {
        return wrapped_alarm->cancel();
    }

    mir::time::Alarm::State state() const override
    {
        return wrapped_alarm->state();
    }

    bool reschedule_in(std::chrono::milliseconds delay) override
    {
        return wrapped_alarm->reschedule_in(delay);
    }

    bool reschedule_for(mir::time::Timestamp t) override
    {
        return wrapped_alarm->reschedule_for(t);
    }

    std::shared_ptr<mir::time::Alarm> const wrapped_alarm;
};

};

struct detail::AdvanceableAlarm : mir::time::Alarm
{
    AdvanceableAlarm(
        mir::time::Timestamp now,
        std::shared_ptr<mir::LockableCallback> const& callback)
        : now{now}, callback{callback}
    {
    }

    bool cancel() override
    {
        std::lock_guard<std::mutex> lock{mutex};
        state_ = mir::time::Alarm::cancelled;
        return true;
    }

    mir::time::Alarm::State state() const override
    {
        std::lock_guard<std::mutex> lock{mutex};
        return state_;
    }

    bool reschedule_in(std::chrono::milliseconds delay) override
    {
        std::lock_guard<std::mutex> lock{mutex};
        next_trigger_ = now + delay;
        state_ = mir::time::Alarm::pending;
        return true;
    }

    bool reschedule_for(mir::time::Timestamp t) override
    {
        std::lock_guard<std::mutex> lock{mutex};
        next_trigger_ = t;
        state_ = mir::time::Alarm::pending;
        return true;
    }

    void update_now(mir::time::Timestamp t)
    {
        std::unique_lock<std::mutex> lock{mutex};
        now = t;
        if (state_ == mir::time::Alarm::pending && now >= next_trigger_)
        {
            state_ = mir::time::Alarm::triggered;
            lock.unlock();
            callback->lock();
            (*callback)();
            callback->unlock();
        }
    }

    mir::time::Timestamp next_trigger()
    {
        std::lock_guard<std::mutex> lock{mutex};
        return next_trigger_;
    }

private:
    mir::time::Timestamp now;
    std::shared_ptr<mir::LockableCallback> const callback;
    mir::time::Timestamp next_trigger_;
    mutable std::mutex mutex;
    mir::time::Alarm::State state_ = mir::time::Alarm::triggered;
};


std::unique_ptr<mir::time::Alarm> AdvanceableTimer::create_alarm(
    std::function<void()> const& callback)
{
    struct SimpleLockableCallback : public mir::LockableCallback
    {
        SimpleLockableCallback(std::function<void()> const& callback)
            : callback{callback}
        {
        }

        void operator()() override { callback(); }
        void lock() override {}
        void unlock() override {}

        std::function<void()> const callback;
    };

    return create_alarm(std::make_unique<SimpleLockableCallback>(callback));
}

std::unique_ptr<mir::time::Alarm> AdvanceableTimer::create_alarm(
    std::unique_ptr<mir::LockableCallback> callback)
{
    auto const adv_alarm =
        std::make_shared<detail::AdvanceableAlarm>(now(), std::move(callback));

    register_alarm(adv_alarm);

    return std::unique_ptr<AlarmWrapper>(new AlarmWrapper{adv_alarm});
}

void AdvanceableTimer::advance_by(std::chrono::milliseconds advance)
{
    {
        std::lock_guard<std::mutex> lock{now_mutex};
        now_ += advance;
    }
    trigger_alarms();
}

void AdvanceableTimer::register_alarm(
    std::shared_ptr<detail::AdvanceableAlarm> const& alarm)
{
    std::lock_guard<std::mutex> lock{alarms_mutex};
    alarms.push_back(alarm);
}

void AdvanceableTimer::trigger_alarms()
{
    std::unique_lock<std::mutex> lock{alarms_mutex};

    // sort by trigger time
    std::sort(begin(alarms), end(alarms),
              [] (std::weak_ptr<detail::AdvanceableAlarm> const& weak_a1,
                  std::weak_ptr<detail::AdvanceableAlarm> const& weak_a2)
              {

                  auto a1 = weak_a1.lock();
                  auto a2 = weak_a2.lock();

                  if (a1 && a2)
                  {
                      if (a1->next_trigger() == a2->next_trigger())
                          return weak_a1.owner_before(weak_a2);
                      else
                          return a1->next_trigger() < a2->next_trigger();
                  }
                  else
                      return weak_a1.owner_before(weak_a2);
              });

    for (auto& weak_alarm : alarms)
    {
        auto const alarm = weak_alarm.lock();
        if (alarm)
        {
            lock.unlock();
            alarm->update_now(now());
            lock.lock();
        }
    }

    alarms.erase(
        std::remove_if(
            begin(alarms), end(alarms),
            [](std::weak_ptr<detail::AdvanceableAlarm> const& alarm)
            {
                return alarm.expired();
            }),
        end(alarms));
}

mir::time::Timestamp AdvanceableTimer::now() const
{
    std::lock_guard<std::mutex> lock{now_mutex};
    return now_;
}
