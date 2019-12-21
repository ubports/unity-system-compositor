/*
 * Copyright © 2019 UBports
 *
 */

#include "unity_gesture_event_sink.h"
#include "dbus_message_handle.h"

namespace
{
char const* const unity_gesture_name = "com.canonical.Unity.Gesture";
char const* const unity_gesture_path = "/com/canonical/Unity/Gesture";
char const* const unity_gesture_iface = "com.canonical.Unity.Gesture";
}

usc::UnityGestureEventSink::UnityGestureEventSink(
    std::string const& dbus_address)
    : dbus_connection{dbus_address}
{
    dbus_connection.request_name(unity_gesture_name);
}

void usc::UnityGestureEventSink::notify_gesture(std::string const& name)
{
    DBusMessageHandle signal{
        dbus_message_new_signal(
            unity_gesture_path,
            unity_gesture_iface,
            name.c_str())};

    dbus_connection_send(dbus_connection, signal, nullptr);
    dbus_connection_flush(dbus_connection);
}
