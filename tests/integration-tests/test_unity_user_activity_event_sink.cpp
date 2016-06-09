/*
 * Copyright © 2016 Canonical Ltd.
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
 */

#include "src/unity_user_activity_event_sink.h"
#include "src/unity_user_activity_type.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_message_handle.h"

#include "dbus_bus.h"

#include <gtest/gtest.h>

#include <future>

namespace ut = usc::test;

namespace
{

struct AUnityUserActivityEventSink : testing::Test
{
    AUnityUserActivityEventSink()
    {
        connection.add_match(
            "type='signal',"
            "interface='com.canonical.Unity.UserActivity'");
    }

    usc::DBusMessageHandle listen_for_user_activity_signal()
    {
        while (true)
        {
            dbus_connection_read_write(connection, 1);
            auto msg = usc::DBusMessageHandle{dbus_connection_pop_message(connection)};

            if (msg && dbus_message_is_signal(msg, unity_power_button_iface, "Activity"))
            {
                return msg;
            }
        }

    }

    ut::DBusBus bus;
    usc::UnityUserActivityEventSink sink{bus.address()};
    usc::DBusConnectionHandle connection{bus.address().c_str()};

    char const* const unity_power_button_iface = "com.canonical.Unity.UserActivity";
};

}

TEST_F(AUnityUserActivityEventSink, sends_activity_changing_power_state_signal)
{
     auto async_message = std::async(std::launch::async,
        [&] { return listen_for_user_activity_signal(); });

    sink.notify_activity_changing_power_state();

    auto message = async_message.get();         

    int32_t type{-1};
    dbus_message_get_args(message, nullptr,
        DBUS_TYPE_INT32, &type,
        DBUS_TYPE_INVALID);

    EXPECT_EQ(static_cast<int32_t>(usc::UnityUserActivityType::changing_power_state), type);
}

TEST_F(AUnityUserActivityEventSink, sends_activity_extending_power_state_signal)
{
     auto async_message = std::async(std::launch::async,
        [&] { return listen_for_user_activity_signal(); });

    sink.notify_activity_extending_power_state();

    auto message = async_message.get();         

    int32_t type{-1};
    dbus_message_get_args(message, nullptr,
        DBUS_TYPE_INT32, &type,
        DBUS_TYPE_INVALID);

    EXPECT_EQ(static_cast<int32_t>(usc::UnityUserActivityType::extending_power_state), type);
}

