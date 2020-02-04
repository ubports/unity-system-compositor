/*
 * Copyright © 2020 UBports Foundation.
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

#include "unity_gesture_event_sink.h"
#include "dbus_message_handle.h"

namespace
{
char const* const unity_gesture_name = "com.ubports.Unity.Gestures";
char const* const unity_gesture_path = "/com/ubports/Unity/Gestures";
char const* const unity_gesture_iface = "com.ubports.Unity.Gestures";
}

usc::UnityGestureEventSink::UnityGestureEventSink(
    std::string const& dbus_address)
    : dbus_connection{dbus_address}
{
    dbus_connection.request_name(unity_gesture_name);
}

void usc::UnityGestureEventSink::notify_gesture(std::string const& name)
{
    const char * gesture = name.c_str();

    DBusMessageHandle signal{
        dbus_message_new_signal(
            unity_gesture_path,
            unity_gesture_iface,
            "Gesture"),
        DBUS_TYPE_STRING, &gesture,
        DBUS_TYPE_INVALID};

    dbus_connection_send(dbus_connection, signal, nullptr);
    dbus_connection_flush(dbus_connection);
}
