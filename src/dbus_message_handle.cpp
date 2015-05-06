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

#include "dbus_message_handle.h"

#include <stdexcept>
#include <boost/throw_exception.hpp>

usc::DBusMessageHandle::DBusMessageHandle(DBusMessage* message)
    : message{message}
{
}

usc::DBusMessageHandle::DBusMessageHandle(::DBusMessage* message, int first_arg_type, ...)
    : message{message}
{
    if (!message)
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid dbus message"));

    va_list args;
    va_start(args, first_arg_type);
    auto appended = dbus_message_append_args_valist(message, first_arg_type, args);
    va_end(args);

    if (!appended)
    {
        dbus_message_unref(message);
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_message_append_args_valist: Failed to append args"));
    }
}

usc::DBusMessageHandle::DBusMessageHandle(
    ::DBusMessage* message, int first_arg_type, va_list args)
    : message{message}
{
    if (!message)
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid dbus message"));

    if (!dbus_message_append_args_valist(message, first_arg_type, args))
    {
        dbus_message_unref(message);
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_message_append_args_valist: Failed to append args"));
    }
}

usc::DBusMessageHandle::DBusMessageHandle(DBusMessageHandle&& other) noexcept
    : message{other.message}
{
    other.message = nullptr;
}

usc::DBusMessageHandle::~DBusMessageHandle()
{
    if (message)
        dbus_message_unref(message);
}

usc::DBusMessageHandle::operator ::DBusMessage*() const
{
    return message;
}

usc::DBusMessageHandle::operator bool() const
{
    return message != nullptr;
}
