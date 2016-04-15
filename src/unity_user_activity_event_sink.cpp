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

#include "unity_user_activity_event_sink.h"
#include "dbus_message_handle.h"

namespace
{
char const* const unity_user_activity_name = "com.canonical.Unity.UserActivity";
char const* const unity_user_activity_path = "/com/canonical/Unity/UserActivity";
char const* const unity_user_activity_iface = "com.canonical.Unity.UserActivity";
}

usc::UnityUserActivityEventSink::UnityUserActivityEventSink(
    std::string const& dbus_address)
    : dbus_connection{dbus_address}
{
    dbus_connection.request_name(unity_user_activity_name);
}

void usc::UnityUserActivityEventSink::notify_activity_changing_power_state()
{
    int const changing_power_state = 0;

    DBusMessageHandle signal{
        dbus_message_new_signal(
            unity_user_activity_path,
            unity_user_activity_iface,
            "Activity"),
        DBUS_TYPE_INT32, &changing_power_state,
        DBUS_TYPE_INVALID};

    dbus_connection_send(dbus_connection, signal, nullptr);
    dbus_connection_flush(dbus_connection);
}

void usc::UnityUserActivityEventSink::notify_activity_extending_power_state()
{
    int const extending_power_state = 0;

    DBusMessageHandle signal{
        dbus_message_new_signal(
            unity_user_activity_path,
            unity_user_activity_iface,
            "Activity"),
        DBUS_TYPE_INT32, &extending_power_state,
        DBUS_TYPE_INVALID};

    dbus_connection_send(dbus_connection, signal, nullptr);
    dbus_connection_flush(dbus_connection);
}
