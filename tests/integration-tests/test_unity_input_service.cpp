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
#include "src/unity_input_service_introspection.h"

#include "wait_condition.h"
#include "dbus_bus.h"
#include "dbus_client.h"
#include "unity_input_dbus_client.h"

#include "usc/test/mock_input_configuration.h"

#include <stdexcept>
#include <memory>

namespace ut = usc::test;

namespace
{

struct AUnityInputService : testing::Test
{
    std::chrono::seconds const default_timeout{3};
    ut::DBusBus bus;

    std::shared_ptr<ut::MockInputConfiguration> const mock_input_configuration =
        std::make_shared<testing::NiceMock<ut::MockInputConfiguration>>();
    ut::UnityInputDBusClient client{bus.address()};
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

