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

#ifndef USC_TEST_UNITY_INPUT_DBUS_CLIENT_H_
#define USC_TEST_UNITY_INPUT_DBUS_CLIENT_H_

#include "dbus_client.h"

namespace usc
{
namespace test
{
class UnityInputDBusClient : public DBusClient
{
public:
    UnityInputDBusClient(std::string const& address);
    DBusAsyncReplyString request_introspection();
    DBusAsyncReplyVoid request(char const* requestName, int32_t value);
    DBusAsyncReplyVoid request_set_mouse_primary_button(int32_t button);
    DBusAsyncReplyVoid request_set_touchpad_primary_button(int32_t button);
    DBusAsyncReplyVoid request(char const* requestName, double value);
    DBusAsyncReplyVoid request_set_mouse_cursor_speed(double speed);
    DBusAsyncReplyVoid request_set_mouse_scroll_speed(double speed);
    DBusAsyncReplyVoid request_set_touchpad_cursor_speed(double speed);
    DBusAsyncReplyVoid request_set_touchpad_scroll_speed(double speed);
    DBusAsyncReplyVoid request(char const* requestName, bool value);
    DBusAsyncReplyVoid request_set_touchpad_two_finger_scroll(bool enabled);
    DBusAsyncReplyVoid request_set_touchpad_tap_to_click(bool enabled);
    DBusAsyncReplyVoid request_set_touchpad_disable_with_mouse(bool enabled);
    DBusAsyncReplyVoid request_set_touchpad_disable_while_typing(bool enabled);
    char const* const unity_input_interface = "com.canonical.Unity.Input";
};

}
}

#endif
