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

#include "dbus_client.h"

#include "src/dbus_message_handle.h"

#include <stdexcept>

namespace ut = usc::test;

ut::DBusAsyncReply::DBusAsyncReply(DBusPendingCall* pending_reply)
    : pending_reply{pending_reply}
{
}

ut::DBusAsyncReply::DBusAsyncReply(DBusAsyncReply&& other)
    : pending_reply{other.pending_reply}
{
    other.pending_reply = nullptr;
}

ut::DBusAsyncReply::~DBusAsyncReply()
{
    throw_on_error_reply(get());
}

usc::DBusMessageHandle ut::DBusAsyncReply::get()
{
    if (pending_reply)
    {
        dbus_pending_call_block(pending_reply);
        auto reply = usc::DBusMessageHandle{
            dbus_pending_call_steal_reply(pending_reply)};
        dbus_pending_call_unref(pending_reply);
        pending_reply = nullptr;

        return reply;
    }
    else
    {
        return usc::DBusMessageHandle{nullptr};
    }
}

void ut::DBusAsyncReply::throw_on_error_reply(::DBusMessage* reply)
{
    if (reply && dbus_message_get_error_name(reply) != nullptr)
        throw std::runtime_error("Got an error reply");
}

void ut::DBusAsyncReply::throw_on_invalid_reply(::DBusMessage* reply)
{
    if (!reply)
        throw std::runtime_error("Async reply is invalid");
}

void ut::DBusAsyncReplyVoid::get()
{
    auto reply = ut::DBusAsyncReply::get();
    throw_on_invalid_reply(reply);
    throw_on_error_reply(reply);
}

int ut::DBusAsyncReplyInt::get()
{
    auto reply = ut::DBusAsyncReply::get();
    throw_on_invalid_reply(reply);
    throw_on_error_reply(reply);

    int32_t val{-1};
    dbus_message_get_args(reply, nullptr, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
    return val;
}

bool ut::DBusAsyncReplyBool::get()
{
    auto reply = ut::DBusAsyncReply::get();
    throw_on_invalid_reply(reply);
    throw_on_error_reply(reply);

    dbus_bool_t val{FALSE};
    dbus_message_get_args(reply, nullptr, DBUS_TYPE_BOOLEAN, &val, DBUS_TYPE_INVALID);
    return val == TRUE;
}

ut::DBusClient::DBusClient(
    std::string const& bus_address,
    std::string const& destination,
    std::string const& path,
    std::string const& interface)
    : connection{bus_address.c_str()},
      destination{destination},
      path{path},
      interface{interface}
{
}

void ut::DBusClient::DBusClient::disconnect()
{
    if (dbus_connection_get_is_connected(connection))
        dbus_connection_close(connection);
}

::DBusPendingCall* ut::DBusClient::invoke_with_pending(
    const char* method, int first_arg_type, ...)
{
    va_list args;
    va_start(args, first_arg_type);
    usc::DBusMessageHandle msg{
        dbus_message_new_method_call(
            destination.c_str(),
            path.c_str(),
            interface.c_str(),
            method),
        first_arg_type, args};
    va_end(args);

    DBusPendingCall* pending_reply;
    dbus_connection_send_with_reply(
        connection, msg, &pending_reply, 3000);
    dbus_connection_flush(connection);

    return pending_reply;
}
