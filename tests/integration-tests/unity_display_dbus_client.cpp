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
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "unity_display_dbus_client.h"

namespace ut = usc::test;

ut::UnityDisplayDBusClient::UnityDisplayDBusClient(std::string const& address)
    : ut::DBusClient{
        address,
        "com.canonical.Unity.Display",
        "/com/canonical/Unity/Display"}
{
}

ut::DBusAsyncReplyString ut::UnityDisplayDBusClient::request_introspection()
{
    return invoke_with_reply<ut::DBusAsyncReplyString>(
        "org.freedesktop.DBus.Introspectable", "Introspect",
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityDisplayDBusClient::request_turn_on()
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(
        unity_display_interface, "TurnOn",
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityDisplayDBusClient::request_turn_off()
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(
        unity_display_interface, "TurnOff",
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReply ut::UnityDisplayDBusClient::request_invalid_method()
{
    return invoke_with_reply<ut::DBusAsyncReply>(
        unity_display_interface, "invalidMethod", DBUS_TYPE_INVALID);
}
