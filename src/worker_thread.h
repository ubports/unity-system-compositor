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

#ifndef USC_WORKER_THREAD_H_
#define USC_WORKER_THREAD_H_

#include <memory>
#include <string>
#include <thread>

namespace usc
{
class Worker;
class WorkerThread
{
public:
    WorkerThread(std::string name);
    ~WorkerThread();
    void queue_task(std::function<void()> task);
    void queue_task(std::function<void()> task, int id);

private:
    WorkerThread(WorkerThread const&) = delete;
    WorkerThread& operator=(WorkerThread const&) = delete;

    std::unique_ptr<Worker> worker;
    std::thread thread;
};

}

#endif
