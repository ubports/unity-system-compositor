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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */
#include "src/unity_input_service.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_connection_thread.h"
#include "src/dbus_event_loop.h"
#include "src/dbus_message_handle.h"
#include "src/unity_screen_service.h"
#include "src/unity_input_service_introspection.h"
#include "src/unity_screen_service_introspection.h"

#include "wait_condition.h"
#include "dbus_bus.h"
#include "dbus_client.h"
#include "unity_input_dbus_client.h"
#include "unity_screen_dbus_client.h"

#include "usc/test/mock_input_configuration.h"
#include "usc/test/mock_screen.h"

namespace ut = usc::test;
using namespace testing;

namespace
{

struct UnityServices : testing::Test
{
    std::chrono::seconds const default_timeout{3};
    ut::DBusBus bus;

    ut::UnityScreenDBusClient screen_client{bus.address()};
    ut::UnityInputDBusClient input_client{bus.address()};
    std::shared_ptr<ut::MockScreen> const mock_screen =
        std::make_shared<testing::NiceMock<ut::MockScreen>>();
    std::shared_ptr<ut::MockInputConfiguration> const mock_input_configuration =
        std::make_shared<testing::NiceMock<ut::MockInputConfiguration>>();
    std::shared_ptr<usc::DBusEventLoop> const dbus_loop=
        std::make_shared<usc::DBusEventLoop>();
    usc::UnityScreenService screen_service{dbus_loop, bus.address(), mock_screen};
    usc::UnityInputService input_service{dbus_loop, bus.address(), mock_input_configuration};
    std::shared_ptr<usc::DBusConnectionThread> const dbus_thread =
        std::make_shared<usc::DBusConnectionThread>(dbus_loop);
};

}

TEST_F(UnityServices, offer_screen_introspection)
{
    auto reply = screen_client.request_introspection();
    EXPECT_THAT(reply.get(), Eq(unity_screen_service_introspection));
}

TEST_F(UnityServices, offer_input_introspection)
{
    auto reply = input_client.request_introspection();
    EXPECT_THAT(reply.get(), Eq(unity_input_service_introspection));
}

TEST_F(UnityServices, provides_access_to_input_methods)
{
    double const speed = 8.0;

    EXPECT_CALL(*mock_input_configuration, set_mouse_scroll_speed(speed));

    input_client.request_set_mouse_scroll_speed(speed);
}
