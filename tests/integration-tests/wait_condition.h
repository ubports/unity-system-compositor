/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#ifndef USC_WAIT_CONDITION_H_
#define USC_WAIT_CONDITION_H_

#include <gmock/gmock.h>

#include <chrono>
#include <mutex>
#include <condition_variable>

namespace usc
{
namespace test
{

struct WaitCondition
{
    void wait_for(std::chrono::milliseconds msec)
    {
        std::unique_lock<std::mutex> ul(guard);
        condition.wait_for(ul, msec, [this] { return woken_; });
    }

    void wake_up()
    {
        std::lock_guard<std::mutex> ul(guard);
        woken_ = true;
        condition.notify_all();
    }

    bool woken()
    {
        std::lock_guard<std::mutex> ul(guard);
        return woken_;
    }

private:
    std::mutex guard;
    std::condition_variable condition;
    bool woken_ = false;
};

ACTION_P(WakeUp, wait_condition)
{
    wait_condition->wake_up();
}
ACTION_P2(WaitFor, wait_condition, delay)
{
    wait_condition->wait_for(delay);
}

}
}

#endif
