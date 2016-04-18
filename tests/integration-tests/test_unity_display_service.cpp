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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/unity_display_service.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_connection_thread.h"
#include "src/dbus_event_loop.h"
#include "src/dbus_message_handle.h"
#include "src/screen.h"
#include "src/unity_display_service_introspection.h"
#include "wait_condition.h"
#include "dbus_bus.h"
#include "dbus_client.h"
#include "unity_display_dbus_client.h"

#include "usc/test/mock_screen.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>
#include <memory>

namespace ut = usc::test;

namespace
{

struct AUnityDisplayService : testing::Test
{
    ut::DBusBus bus;

    std::shared_ptr<ut::MockScreen> const mock_screen =
        std::make_shared<testing::NiceMock<ut::MockScreen>>();
    ut::UnityDisplayDBusClient client{bus.address()};
    std::shared_ptr<usc::DBusEventLoop> const dbus_loop =
        std::make_shared<usc::DBusEventLoop>();
    usc::UnityDisplayService service{dbus_loop, bus.address(), mock_screen};
    std::shared_ptr<usc::DBusConnectionThread> const dbus_thread =
        std::make_shared<usc::DBusConnectionThread>(dbus_loop);
};

}

TEST_F(AUnityDisplayService, replies_to_introspection_request)
{
    using namespace testing;

    auto reply = client.request_introspection();
    EXPECT_THAT(reply.get(), Eq(unity_display_service_introspection));
}

TEST_F(AUnityDisplayService, forwards_turn_on_request)
{
    EXPECT_CALL(*mock_screen, turn_on());

    client.request_turn_on();
}

TEST_F(AUnityDisplayService, forwards_turn_off_request)
{
    EXPECT_CALL(*mock_screen, turn_off());

    client.request_turn_off();
}

TEST_F(AUnityDisplayService, returns_error_reply_for_unsupported_method)
{
    using namespace testing;

    auto reply = client.request_invalid_method();
    auto reply_msg = reply.get();

    EXPECT_THAT(dbus_message_get_type(reply_msg), Eq(DBUS_MESSAGE_TYPE_ERROR));
    EXPECT_THAT(dbus_message_get_error_name(reply_msg), StrEq(DBUS_ERROR_FAILED));
}
