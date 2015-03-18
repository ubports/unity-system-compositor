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

#ifndef USC_DBUS_MESSAGE_HANDLE_H_
#define USC_DBUS_MESSAGE_HANDLE_H_

#include <dbus/dbus.h>
#include <cstdarg>

namespace usc
{

class DBusMessageHandle
{
public:
    DBusMessageHandle(DBusMessage* message);
    DBusMessageHandle(DBusMessage* message, int first_arg_type, ...);
    DBusMessageHandle(DBusMessage* message, int first_arg_type, va_list args);
    DBusMessageHandle(DBusMessageHandle&&) noexcept;
    ~DBusMessageHandle();

    operator ::DBusMessage*() const;
    operator bool() const;

private:
    DBusMessageHandle(DBusMessageHandle const&) = delete;
    DBusMessageHandle& operator=(DBusMessageHandle const&) = delete;
    ::DBusMessage* message;
};

}

#endif
