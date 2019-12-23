/*
 * Copyright © 2019 UBports
 *
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
