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

#include "src/unity_input_service.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_connection_thread.h"
#include "src/dbus_event_loop.h"
#include "src/dbus_message_handle.h"
#include "src/input_configuration.h"
#include "src/unity_input_service_introspection.h"
#include "wait_condition.h"
#include "dbus_bus.h"
#include "dbus_client.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>
#include <memory>

namespace ut = usc::test;

namespace
{

struct MockInputConfiguration : usc::InputConfiguration
{
public:
    MOCK_METHOD1(set_mouse_primary_button, void(int32_t));
    MOCK_METHOD1(set_mouse_cursor_speed, void(double));
    MOCK_METHOD1(set_mouse_scroll_speed, void(double));
    MOCK_METHOD1(set_touchpad_primary_button, void(int32_t));
    MOCK_METHOD1(set_touchpad_cursor_speed, void(double));
    MOCK_METHOD1(set_touchpad_scroll_speed, void(double));
    MOCK_METHOD1(set_two_finger_scroll, void(bool));
    MOCK_METHOD1(set_tap_to_click, void(bool));
    MOCK_METHOD1(set_disable_touchpad_with_mouse, void(bool));
    MOCK_METHOD1(set_disable_touchpad_while_typing, void(bool));
};

class UnityInputDBusClient : public ut::DBusClient
{
public:
    UnityInputDBusClient(std::string const& address)
        : ut::DBusClient{
            address,
            "com.canonical.Unity.Input",
            "/com/canonical/Unity/Input"}
    {
    }

    ut::DBusAsyncReplyString request_introspection()
    {
        return invoke_with_reply<ut::DBusAsyncReplyString>(
            "org.freedesktop.DBus.Introspectable", "Introspect",
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request(char const* requestName, int32_t value)
    {
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_input_interface, requestName,
            DBUS_TYPE_INT32, &value,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_set_mouse_primary_button(int32_t button)
    {
        return request("setMousePrimaryButton", button);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_primary_button(int32_t button)
    {
        return request("setTouchpadPrimaryButton", button);
    }

    ut::DBusAsyncReplyVoid request(char const* requestName, double value)
    {
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_input_interface, requestName,
            DBUS_TYPE_DOUBLE, &value,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_set_mouse_cursor_speed(double speed)
    {
        return request("setMouseCursorSpeed", speed);
    }

    ut::DBusAsyncReplyVoid request_set_mouse_scroll_speed(double speed)
    {
        return request("setMouseScrollSpeed", speed);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_cursor_speed(double speed)
    {
        return request("setTouchpadCursorSpeed", speed);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_scroll_speed(double speed)
    {
        return request("setTouchpadScrollSpeed", speed);
    }

    ut::DBusAsyncReplyVoid request(char const* requestName, bool value)
    {
        dbus_bool_t copied = value;
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_input_interface, requestName,
            DBUS_TYPE_BOOLEAN, &copied,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_two_finger_scroll(bool enabled)
    {
        return request("setTouchpadTwoFingerScroll", enabled);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_tap_to_click(bool enabled)
    {
        return request("setTouchpadTapToClick", enabled);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_disable_with_mouse(bool enabled)
    {
        return request("setTouchpadDisableWithMouse", enabled);
    }

    ut::DBusAsyncReplyVoid request_set_touchpad_disable_while_typing(bool enabled)
    {
        return request("setTouchpadDisableWhileTyping", enabled);
    }

    char const* const unity_input_interface = "com.canonical.Unity.Input";
};

struct AUnityInputService : testing::Test
{
    std::chrono::seconds const default_timeout{3};
    ut::DBusBus bus;

    std::shared_ptr<MockInputConfiguration> const mock_input_configuration =
        std::make_shared<testing::NiceMock<MockInputConfiguration>>();
    UnityInputDBusClient client{bus.address()};
    std::shared_ptr<usc::DBusEventLoop> const dbus_loop=
        std::make_shared<usc::DBusEventLoop>();
    usc::UnityInputService service{dbus_loop, bus.address(), mock_input_configuration};
    std::shared_ptr<usc::DBusConnectionThread> const dbus_thread =
        std::make_shared<usc::DBusConnectionThread>(dbus_loop);
};

}

TEST_F(AUnityInputService, replies_to_introspection_request)
{
    using namespace testing;

    auto reply = client.request_introspection();
    EXPECT_THAT(reply.get(), Eq(unity_input_service_introspection));
}

TEST_F(AUnityInputService, forwards_set_mouse_primary_button)
{
    int32_t const primary_button = 2;

    EXPECT_CALL(*mock_input_configuration, set_mouse_primary_button(primary_button));

    client.request_set_mouse_primary_button(primary_button);
}

TEST_F(AUnityInputService, forwards_set_mouse_cursor_speed)
{
    double const speed = 10.0;

    EXPECT_CALL(*mock_input_configuration, set_mouse_cursor_speed(speed));

    client.request_set_mouse_cursor_speed(speed);
}

TEST_F(AUnityInputService, forwards_set_mouse_scroll_speed)
{
    double const speed = 8.0;

    EXPECT_CALL(*mock_input_configuration, set_mouse_scroll_speed(speed));

    client.request_set_mouse_scroll_speed(speed);
}

TEST_F(AUnityInputService, forwards_set_touchpad_primary_button)
{
    int32_t const primary_button = 1;

    EXPECT_CALL(*mock_input_configuration, set_touchpad_primary_button(primary_button));

    client.request_set_touchpad_primary_button(primary_button);
}

TEST_F(AUnityInputService, forwards_set_touchpad_cursor_speed)
{
    double const speed = 10.0;

    EXPECT_CALL(*mock_input_configuration, set_touchpad_cursor_speed(speed));

    client.request_set_touchpad_cursor_speed(speed);
}

TEST_F(AUnityInputService, forwards_set_touchpad_scroll_speed)
{
    double const speed = 8.0;

    EXPECT_CALL(*mock_input_configuration, set_touchpad_scroll_speed(speed));

    client.request_set_touchpad_scroll_speed(speed);
}

TEST_F(AUnityInputService, forwards_set_disable_touchpad_while_typing)
{
    bool const disable_it = false;

    EXPECT_CALL(*mock_input_configuration, set_disable_touchpad_while_typing(disable_it));

    client.request_set_touchpad_disable_while_typing(disable_it);
}

TEST_F(AUnityInputService, forwards_set_disable_touchpad_with_mouse)
{
    bool const enable_it = true;

    EXPECT_CALL(*mock_input_configuration, set_disable_touchpad_with_mouse(enable_it));

    client.request_set_touchpad_disable_with_mouse(enable_it);
}

TEST_F(AUnityInputService, forwards_set_two_finger_scroll)
{
    bool const enable_it = true;

    EXPECT_CALL(*mock_input_configuration, set_two_finger_scroll(enable_it));

    client.request_set_touchpad_two_finger_scroll(enable_it);
}

TEST_F(AUnityInputService, forwards_set_touchpad_tap_to_click)
{
    bool const enable_it = true;

    EXPECT_CALL(*mock_input_configuration, set_tap_to_click(enable_it));

    client.request_set_touchpad_tap_to_click(enable_it);
}

