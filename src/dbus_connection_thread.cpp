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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "dbus_connection_thread.h"
#include "dbus_event_loop.h"
#include "thread_name.h"

usc::DBusConnectionThread::DBusConnectionThread(std::shared_ptr<DBusEventLoop> const& loop) : dbus_event_loop(loop)
{
    std::promise<void> event_loop_started;
    auto event_loop_started_future = event_loop_started.get_future();

    dbus_loop_thread = std::thread(
        [this,&event_loop_started]
        {
            usc::set_thread_name("USC/DBus");
            dbus_event_loop->run(event_loop_started);
        });

    event_loop_started_future.wait();
}

usc::DBusConnectionThread::~DBusConnectionThread()
{
    dbus_event_loop->stop();
    dbus_loop_thread.join();
}

usc::DBusEventLoop & usc::DBusConnectionThread::loop()
{
    return *dbus_event_loop;
}
