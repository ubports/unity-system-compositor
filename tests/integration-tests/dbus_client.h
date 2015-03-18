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

#ifndef USC_TEST_DBUS_CLIENT_H_
#define USC_TEST_DBUS_CLIENT_H_

#include "src/dbus_connection_handle.h"

#include <string>

#include <dbus/dbus.h>

namespace usc
{
class DBusMessageHandle;

namespace test
{

class DBusAsyncReply
{
public:
    DBusAsyncReply(DBusAsyncReply const&) = delete;
    DBusAsyncReply& operator=(DBusAsyncReply const&) = delete;

    DBusAsyncReply(DBusPendingCall* pending_reply);
    DBusAsyncReply(DBusAsyncReply&& other);

    ~DBusAsyncReply();

    usc::DBusMessageHandle get();

protected:
    void throw_on_error_reply(::DBusMessage* reply);
    void throw_on_invalid_reply(::DBusMessage* reply);

private:
    DBusPendingCall* pending_reply = nullptr;
};

class DBusAsyncReplyVoid : DBusAsyncReply
{
public:
    using DBusAsyncReply::DBusAsyncReply;
    void get();
};

class DBusAsyncReplyInt : DBusAsyncReply
{
public:
    using DBusAsyncReply::DBusAsyncReply;
    int get();
};

class DBusAsyncReplyBool : DBusAsyncReply
{
public:
    using DBusAsyncReply::DBusAsyncReply;
    bool get();
};

class DBusClient
{
public:
    DBusClient(
        std::string const& bus_address,
        std::string const& destination,
        std::string const& path);

    void disconnect();

    template <typename T, typename... Args>
    T invoke_with_reply(
        char const* interface, char const* method, int first_arg_type, Args... args)
    {
        return T{invoke_with_pending(interface, method, first_arg_type, args...)};
    }

protected:
    DBusPendingCall* invoke_with_pending(
        char const* interface, char const* method, int first_arg_type, ...);
    usc::DBusConnectionHandle connection;
    std::string const destination;
    std::string const path;
    std::string const interface;
};

}
}

#endif
