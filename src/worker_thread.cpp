/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#include "worker_thread.h"
#include <mir/run_mir.h>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <map>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>

namespace usc
{

class Worker
{
public:
    Worker(std::string name)
        : name{name.substr(0, 15)}, exiting{false}
    {
    }

    ~Worker()
    {
        exit();
    }

    void operator()() noexcept
    try
    {
       pthread_setname_np(pthread_self(), name.c_str());

       std::unique_lock<std::mutex> lock{state_mutex};
       while (!exiting)
       {
           task_available_cv.wait(lock, [&]{ return exiting || !tasks.empty(); });

           if (!exiting)
           {
               auto& task = tasks.front();

               lock.unlock();
               task();
               lock.lock();
               tasks.pop_front();
           }
       }
    }
    catch(...)
    {
        mir::terminate_with_current_exception();
    }

    void queue_task(std::function<void()> task)
    {
        std::lock_guard<std::mutex> lock{state_mutex};
        tasks.push_back(std::move(task));
        task_available_cv.notify_one();
    }

    void queue_task(std::function<void()> task, int id)
    {
        std::lock_guard<std::mutex> lock{state_mutex};
        coalesced_tasks[id] = task;
        tasks.push_back([this, id]{ run_coalesced_task(id);});
        task_available_cv.notify_one();
    }

    void run_coalesced_task(int id)
    {
        std::unique_lock<std::mutex> lock{state_mutex};
        auto it = coalesced_tasks.find(id);
        if (it != coalesced_tasks.end())
        {
            auto task = it->second;
            coalesced_tasks.erase(it);
            lock.unlock();
            task();
        }
    }

    void exit()
    {
        std::lock_guard<std::mutex> lock{state_mutex};
        exiting = true;
        task_available_cv.notify_one();
    }

private:
    std::mutex mutable state_mutex;
    std::string name;
    bool exiting;
    std::map<int, std::function<void()>> coalesced_tasks;
    std::deque<std::function<void()>> tasks;
    std::condition_variable task_available_cv;
};

}

usc::WorkerThread::WorkerThread(std::string name)
    : worker{new Worker(name)},
      thread{std::ref(*worker)}
{
}

usc::WorkerThread::~WorkerThread()
{
    worker->exit();
    if (thread.joinable())
        thread.join();
}

void usc::WorkerThread::queue_task(std::function<void()> task)
{
    worker->queue_task(std::move(task));
}

void usc::WorkerThread::queue_task(std::function<void()> task, int id)
{
    worker->queue_task(std::move(task), id);
}
