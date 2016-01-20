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

#include "unity_screen_dbus_client.h"
#include "src/dbus_message_handle.h"

namespace ut = usc::test;

ut::UnityScreenDBusClient::UnityScreenDBusClient(std::string const& address)
    : ut::DBusClient{
        address,
        "com.canonical.Unity.Screen",
        "/com/canonical/Unity/Screen"}
{
    connection.add_match(
        "type='signal',"
        "interface='com.canonical.Unity.Screen',"
        "member='DisplayPowerStateChange'");
}

ut::DBusAsyncReplyString ut::UnityScreenDBusClient::request_introspection()
{
    return invoke_with_reply<ut::DBusAsyncReplyString>("org.freedesktop.DBus.Introspectable", "Introspect",
                                                       DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityScreenDBusClient::request_set_user_brightness(int32_t brightness)
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(unity_screen_interface, "setUserBrightness", DBUS_TYPE_INT32,
                                                     &brightness, DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityScreenDBusClient::request_user_auto_brightness_enable(bool enabled)
{
    dbus_bool_t const e = enabled ? TRUE : FALSE;
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(unity_screen_interface, "userAutobrightnessEnable",
                                                     DBUS_TYPE_BOOLEAN, &e, DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityScreenDBusClient::request_set_inactivity_timeouts(int32_t poweroff_timeout,
                                                                                  int32_t dimmer_timeout)
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(unity_screen_interface, "setInactivityTimeouts", DBUS_TYPE_INT32,
                                                     &poweroff_timeout, DBUS_TYPE_INT32, &dimmer_timeout,
                                                     DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityScreenDBusClient::request_set_touch_visualization_enabled(bool enabled)
{
    dbus_bool_t const e = enabled ? TRUE : FALSE;
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(unity_screen_interface, "setTouchVisualizationEnabled",
                                                     DBUS_TYPE_BOOLEAN, &e, DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyBool ut::UnityScreenDBusClient::request_set_screen_power_mode(std::string const& mode, int reason)
{
    auto mode_cstr = mode.c_str();
    return invoke_with_reply<ut::DBusAsyncReplyBool>(unity_screen_interface, "setScreenPowerMode", DBUS_TYPE_STRING,
                                                     &mode_cstr, DBUS_TYPE_INT32, &reason, DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyInt ut::UnityScreenDBusClient::request_keep_display_on()
{
    return invoke_with_reply<ut::DBusAsyncReplyInt>(unity_screen_interface, "keepDisplayOn", DBUS_TYPE_INVALID);
}

ut::DBusAsyncReplyVoid ut::UnityScreenDBusClient::request_remove_display_on_request(int id)
{
    return invoke_with_reply<ut::DBusAsyncReplyVoid>(unity_screen_interface, "removeDisplayOnRequest", DBUS_TYPE_INT32,
                                                     &id, DBUS_TYPE_INVALID);
}

ut::DBusAsyncReply ut::UnityScreenDBusClient::request_invalid_method()
{
    return invoke_with_reply<ut::DBusAsyncReply>(unity_screen_interface, "invalidMethod", DBUS_TYPE_INVALID);
}

ut::DBusAsyncReply ut::UnityScreenDBusClient::request_method_with_invalid_arguments()
{
    char const* const str = "abcd";
    return invoke_with_reply<ut::DBusAsyncReply>(unity_screen_interface, "setUserBrightness", DBUS_TYPE_STRING, &str,
                                                 DBUS_TYPE_INVALID);
}

usc::DBusMessageHandle ut::UnityScreenDBusClient::listen_for_power_state_change_signal()
{
    while (true)
    {
        dbus_connection_read_write(connection, 1);
        auto msg = usc::DBusMessageHandle{dbus_connection_pop_message(connection)};

        if (msg && dbus_message_is_signal(msg, "com.canonical.Unity.Screen", "DisplayPowerStateChange"))
        {
            return msg;
        }
    }
}
