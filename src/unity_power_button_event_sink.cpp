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

#include "unity_power_button_event_sink.h"
#include "dbus_message_handle.h"

namespace
{
char const* const unity_power_button_name = "com.canonical.Unity.PowerButton";
char const* const unity_power_button_path = "/com/canonical/Unity/PowerButton";
char const* const unity_power_button_iface = "com.canonical.Unity.PowerButton";
}

usc::UnityPowerButtonEventSink::UnityPowerButtonEventSink(
    std::string const& dbus_address)
    : dbus_connection{dbus_address}
{
    dbus_connection.request_name(unity_power_button_name);
}

void usc::UnityPowerButtonEventSink::notify_press()
{
    DBusMessageHandle signal{
        dbus_message_new_signal(
            unity_power_button_path,
            unity_power_button_iface,
            "Press")};

    dbus_connection_send(dbus_connection, signal, nullptr);
    dbus_connection_flush(dbus_connection);
}

void usc::UnityPowerButtonEventSink::notify_release()
{
    DBusMessageHandle signal{
        dbus_message_new_signal(
            unity_power_button_path,
            unity_power_button_iface,
            "Release")};

    dbus_connection_send(dbus_connection, signal, nullptr);
    dbus_connection_flush(dbus_connection);
}
