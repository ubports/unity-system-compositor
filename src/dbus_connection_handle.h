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

#ifndef USC_DBUS_CONNECTION_HANDLE_H_
#define USC_DBUS_CONNECTION_HANDLE_H_

#include <dbus/dbus.h>

#include <string>

namespace usc
{

class DBusConnectionHandle
{
public:
    DBusConnectionHandle(std::string const& address);
    ~DBusConnectionHandle();

    void request_name(char const* name) const;
    void add_match(char const* match) const;
    void add_filter(DBusHandleMessageFunction filter_func, void* user_data) const;

    operator ::DBusConnection*() const;

private:
    DBusConnectionHandle(DBusConnectionHandle const&) = delete;
    DBusConnectionHandle& operator=(DBusConnectionHandle const&) = delete;

    ::DBusConnection* connection;
};

}

#endif
