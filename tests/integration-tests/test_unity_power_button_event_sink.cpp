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

#include "src/unity_power_button_event_sink.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_message_handle.h"

#include "dbus_bus.h"

#include <gtest/gtest.h>

#include <future>

namespace ut = usc::test;

namespace
{

struct AUnityPowerButtonEventSink : testing::Test
{
    AUnityPowerButtonEventSink()
    {
        connection.add_match(
            "type='signal',"
            "interface='com.canonical.Unity.PowerButton'");
    }

    usc::DBusMessageHandle listen_for_power_button_signal(char const* name)
    {
        while (true)
        {
            dbus_connection_read_write(connection, 1);
            auto msg = usc::DBusMessageHandle{dbus_connection_pop_message(connection)};

            if (msg && dbus_message_is_signal(msg, unity_power_button_iface, name))
            {
                return msg;
            }
        }

    }

    ut::DBusBus bus;
    usc::UnityPowerButtonEventSink sink{bus.address()};
    usc::DBusConnectionHandle connection{bus.address().c_str()};

    char const* const unity_power_button_iface = "com.canonical.Unity.PowerButton";
};

}

TEST_F(AUnityPowerButtonEventSink, sends_press_signal)
{
     auto async_message = std::async(std::launch::async,
        [&] { return listen_for_power_button_signal("Press"); });

    sink.notify_press();

    async_message.get();         
}

TEST_F(AUnityPowerButtonEventSink, sends_release_signal)
{
     auto async_message = std::async(std::launch::async,
        [&] { return listen_for_power_button_signal("Release"); });

    sink.notify_release();

    async_message.get();         
}
