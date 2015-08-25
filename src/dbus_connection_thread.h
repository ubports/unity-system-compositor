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

#ifndef USC_DBUS_CONNECTION_THREAD_H_
#define USC_DBUS_CONNECTION_THREAD_H_

#include "dbus_connection_handle.h"
#include "dbus_event_loop.h"

#include <thread>

namespace usc
{

class DBusConnectionThread
{
public:
    DBusConnectionThread(std::string const& address);
    ~DBusConnectionThread();
    DBusConnectionHandle const& connection() const;
    DBusEventLoop & loop();

private:
    DBusConnectionHandle dbus_connection;
    DBusEventLoop dbus_event_loop;
    std::thread dbus_loop_thread;
};

}

#endif
