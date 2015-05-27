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

#include "src/dbus_event_loop.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_message_handle.h"
#include "src/scoped_dbus_error.h"
#include "dbus_bus.h"
#include "dbus_client.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace ut = usc::test;

namespace
{

char const* const test_service_name = "com.TestService";
char const* const test_service_path = "/com/TestService";
char const* const test_service_interface = "com.TestService";

class TestDBusClient : public ut::DBusClient
{
public:
    TestDBusClient(std::string const& address)
        : ut::DBusClient{
            address,
            test_service_name,
            test_service_path}
    {
        std::string const match =
            std::string{"type='signal',interface='"} +
            std::string{test_service_interface} +
            std::string{"',member='signal'"};
        connection.add_match(match.c_str());
    }

    ut::DBusAsyncReplyInt request_add(int32_t a, int32_t b)
    {
        return invoke_with_reply<ut::DBusAsyncReplyInt>(
            test_service_interface, "add",
            DBUS_TYPE_INT32, &a,
            DBUS_TYPE_INT32, &b,
            DBUS_TYPE_INVALID);
    }

    usc::DBusMessageHandle listen_for_signal()
    {
        while (true)
        {
            dbus_connection_read_write(connection, 1);
            auto msg = usc::DBusMessageHandle{dbus_connection_pop_message(connection)};

            if (msg && dbus_message_is_signal(msg, test_service_interface, "signal"))
            {
                return msg;
            }
        }
    }
};

struct ADBusEventLoop : testing::Test
{
    ADBusEventLoop()
    {
        connection.request_name(test_service_name);
        connection.add_filter(handle_dbus_message_thunk, this);

        std::promise<void> event_loop_started;
        auto event_loop_started_future = event_loop_started.get_future();

        dbus_loop_thread = std::thread(
            [this,&event_loop_started]
            {
                dbus_event_loop.run(event_loop_started);
            });

        event_loop_started_future.wait();
    }

    ~ADBusEventLoop()
    {
        dbus_event_loop.stop();
        if (dbus_loop_thread.joinable())
            dbus_loop_thread.join();
    }

    static ::DBusHandlerResult handle_dbus_message_thunk(
        ::DBusConnection* connection, DBusMessage* message, void* user_data)
    {
        auto const a_dbus_event_loop = static_cast<ADBusEventLoop*>(user_data);
        return a_dbus_event_loop->handle_dbus_message(connection, message, user_data);
    }

    DBusHandlerResult handle_dbus_message(
        ::DBusConnection* connection, DBusMessage* message, void* user_data)
    {
        usc::ScopedDBusError args_error;

        if (dbus_message_is_method_call(message, test_service_interface, "add"))
        {
            int32_t a{0};
            int32_t b{0};
            dbus_message_get_args(
                message, &args_error,
                DBUS_TYPE_INT32, &a,
                DBUS_TYPE_INT32, &b,
                DBUS_TYPE_INVALID);

            if (!args_error)
            {
                int32_t const result{a + b};
                usc::DBusMessageHandle reply{
                    dbus_message_new_method_return(message),
                    DBUS_TYPE_INT32, &result,
                    DBUS_TYPE_INVALID};

                dbus_connection_send(connection, reply, nullptr);
            }
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    std::chrono::seconds const default_timeout{3};

    ut::DBusBus bus;

    usc::DBusConnectionHandle connection{bus.address().c_str()};
    usc::DBusEventLoop dbus_event_loop{connection};
    std::thread dbus_loop_thread;
    TestDBusClient client{bus.address()};
};

}

TEST_F(ADBusEventLoop, dispatches_received_message)
{
    using namespace testing;

    int32_t const a = 11;
    int32_t const b = 13;
    int32_t const expected = a + b;

    auto reply = client.request_add(a, b);
    EXPECT_THAT(reply.get(), expected);
}

TEST_F(ADBusEventLoop, enqueues_send_requests)
{
    dbus_event_loop.enqueue(
        [this]
        {
            usc::DBusMessageHandle msg{
                dbus_message_new_signal(
                    test_service_path,
                    test_service_interface,
                    "signal")};

            dbus_connection_send(connection, msg, nullptr);
        });

    auto const reply = client.listen_for_signal();
    EXPECT_TRUE(dbus_message_is_signal(reply, test_service_interface, "signal"));
}

namespace
{

void pending_complete(DBusPendingCall* pending, void* user_data)
{
    auto const pending_promise = static_cast<std::promise<DBusPendingCall*>*>(user_data);
    pending_promise->set_value(pending);

}

}

TEST_F(ADBusEventLoop, handles_reply_timeouts)
{
    using namespace testing;

    std::promise<DBusPendingCall*> pending_promise;
    auto pending_future = pending_promise.get_future();

    static int const timeout_ms = 100;

    auto const start = std::chrono::steady_clock::now();

    dbus_event_loop.enqueue(
        [this,&pending_promise]
        {
            usc::DBusMessageHandle msg{
                dbus_message_new_signal(
                    test_service_path,
                    test_service_interface,
                    "signal")};

            DBusPendingCall* pending{nullptr};
            dbus_connection_send_with_reply(
                connection, msg, &pending, timeout_ms);
            dbus_pending_call_set_notify(
                pending, &pending_complete, &pending_promise, nullptr);
        });

    // No one is going to reply to the signal, so the notification should time out
    auto pending = pending_future.get();
    auto const end = std::chrono::steady_clock::now();
    auto const delay = end - start;

    ASSERT_THAT(pending, NotNull());
    dbus_pending_call_unref(pending);

    // Use a high upper bound for valgrind runs to succeed
    EXPECT_THAT(delay, Lt(std::chrono::milliseconds{timeout_ms * 10}));
    EXPECT_THAT(delay, Ge(std::chrono::milliseconds{timeout_ms}));
}
