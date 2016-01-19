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

#include "dbus_connection_handle.h"
#include "scoped_dbus_error.h"

#include <stdexcept>
#include <boost/throw_exception.hpp>

usc::DBusConnectionHandle::DBusConnectionHandle(std::string const& address)
{
    dbus_threads_init_default();
    ScopedDBusError error;

    connection = dbus_connection_open_private(address.c_str(), &error);
    if (!connection || error)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_connection_open: " + error.message_str()));
    }

    if (!dbus_bus_register(connection, &error))
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_bus_register: " + error.message_str()));
    }
}

usc::DBusConnectionHandle::~DBusConnectionHandle()
{
    if (dbus_connection_get_is_connected(connection))
        dbus_connection_close(connection);
    dbus_connection_unref(connection);
}

void usc::DBusConnectionHandle::request_name(char const* name) const
{
    ScopedDBusError error;

    auto const request_result = dbus_bus_request_name(
        connection, name, DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
    if (error)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_request_name: " + error.message_str()));
    }

    if (request_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_request_name: Failed to become primary owner"));
    }
}

void usc::DBusConnectionHandle::add_match(char const* match) const
{
    ScopedDBusError error;

    dbus_bus_add_match(connection, match, &error);
    if (error)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_add_match: " + error.message_str()));
    }
}

void usc::DBusConnectionHandle::add_filter(
    DBusHandleMessageFunction filter_func,
    void* user_data) const
{
    auto const added = dbus_connection_add_filter(
        connection,
        filter_func,
        user_data,
        nullptr);

    if (!added)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("dbus_connection_add_filter: Failed to add filter"));
    }
}

usc::DBusConnectionHandle::operator ::DBusConnection*() const
{
    return connection;
}
