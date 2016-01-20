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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "unity_input_dbus_client.h"

namespace ut = usc::test;

ut::UnityInputDBusClient::UnityInputDBusClient(std::string const& address)
    : ut::DBusClient{
        address,
        "com.canonical.Unity.Input",
        "/com/canonical/Unity/Input"}
{
}

ut::DBusAsyncReplyString ut::UnityInputDBusClient::request_introspection()
{
    return invoke_with_reply<ut::DBusAsyncReplyString>(
        "org.freedesktop.DBus.Introspectable", "Introspect",
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request(char const* requestName, int32_t value)
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(
        unity_input_interface, requestName,
        DBUS_TYPE_INT32, &value,
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_mouse_primary_button(int32_t button)
{
    return request("setMousePrimaryButton", button);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_primary_button(int32_t button)
{
    return request("setTouchpadPrimaryButton", button);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request(char const* requestName, double value)
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(
        unity_input_interface, requestName,
        DBUS_TYPE_DOUBLE, &value,
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_mouse_cursor_speed(double speed)
{
    return request("setMouseCursorSpeed", speed);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_mouse_scroll_speed(double speed)
{
    return request("setMouseScrollSpeed", speed);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_cursor_speed(double speed)
{
    return request("setTouchpadCursorSpeed", speed);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_scroll_speed(double speed)
{
    return request("setTouchpadScrollSpeed", speed);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request(char const* requestName, bool value)
{
    dbus_bool_t copied = value;
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(
        unity_input_interface, requestName,
        DBUS_TYPE_BOOLEAN, &copied,
        DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_two_finger_scroll(bool enabled)
{
    return request("setTouchpadTwoFingerScroll", enabled);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_tap_to_click(bool enabled)
{
    return request("setTouchpadTapToClick", enabled);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_disable_with_mouse(bool enabled)
{
    return request("setTouchpadDisableWithMouse", enabled);
}

ut::DBusAsyncReplyVoid ut::UnityInputDBusClient::request_set_touchpad_disable_while_typing(bool enabled)
{
    return request("setTouchpadDisableWhileTyping", enabled);
}


