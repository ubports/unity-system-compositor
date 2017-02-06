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

struct FakeScreen : ut::MockScreen
{
    void register_active_outputs_handler(usc::ActiveOutputsHandler const& handler)
    {
        std::lock_guard<std::mutex> lock{active_outputs_mutex};
        active_outputs_handler = handler;
    }

    void notify_active_outputs(usc::ActiveOutputs const& active_outputs)
    {
        std::lock_guard<std::mutex> lock{active_outputs_mutex};
        active_outputs_handler(active_outputs);
    }

    std::mutex active_outputs_mutex;
    usc::ActiveOutputsHandler active_outputs_handler{[](usc::ActiveOutputs const&){}};
};

struct AUnityDisplayService : testing::Test
{
    ut::DBusBus bus;

    std::shared_ptr<FakeScreen> const fake_screen =
        std::make_shared<testing::NiceMock<FakeScreen>>();
    ut::UnityDisplayDBusClient client{bus.address()};
    std::shared_ptr<usc::DBusEventLoop> const dbus_loop =
        std::make_shared<usc::DBusEventLoop>();
    usc::UnityDisplayService service{dbus_loop, bus.address(), fake_screen};
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
    using namespace testing;

    InSequence s;
    EXPECT_CALL(*fake_screen, turn_on(usc::OutputFilter::all));
    EXPECT_CALL(*fake_screen, turn_on(usc::OutputFilter::internal));
    EXPECT_CALL(*fake_screen, turn_on(usc::OutputFilter::external));

    client.request_turn_on("all");
    client.request_turn_on("internal");
    client.request_turn_on("external");
}

TEST_F(AUnityDisplayService, forwards_turn_off_request)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(*fake_screen, turn_off(usc::OutputFilter::all));
    EXPECT_CALL(*fake_screen, turn_off(usc::OutputFilter::internal));
    EXPECT_CALL(*fake_screen, turn_off(usc::OutputFilter::external));

    client.request_turn_off("all");
    client.request_turn_off("internal");
    client.request_turn_off("external");
}

TEST_F(AUnityDisplayService, emits_active_outputs_property_change)
{
    using namespace testing;

    usc::ActiveOutputs expected_active_outputs;
    expected_active_outputs.internal = 2;
    expected_active_outputs.external = 3;

    fake_screen->notify_active_outputs(expected_active_outputs);
    // Received messages are queued at the destination, so it doesn't
    // matter that we start listening after the signal has been sent
    auto message = client.listen_for_properties_changed();

    DBusMessageIter iter;
    dbus_message_iter_init(message, &iter);
    dbus_message_iter_next(&iter);
    DBusMessageIter iter_properties;
    dbus_message_iter_recurse(&iter, &iter_properties);
    DBusMessageIter iter_property;
    dbus_message_iter_recurse(&iter_properties, &iter_property);

    char const* property_name{""};
    usc::ActiveOutputs active_outputs{-1, -1};

    dbus_message_iter_get_basic(&iter_property, &property_name);

    dbus_message_iter_next(&iter_property);
    DBusMessageIter iter_variant;
    DBusMessageIter iter_values;
    dbus_message_iter_recurse(&iter_property, &iter_variant);
    dbus_message_iter_recurse(&iter_variant, &iter_values);

    dbus_message_iter_get_basic(&iter_values, &active_outputs.internal);
    dbus_message_iter_next(&iter_values);
    dbus_message_iter_get_basic(&iter_values, &active_outputs.external);

    EXPECT_THAT(property_name, StrEq("ActiveOutputs"));
    EXPECT_THAT(active_outputs, Eq(expected_active_outputs));
}

TEST_F(AUnityDisplayService, returns_active_outputs_property)
{
    using namespace testing;

    usc::ActiveOutputs expected_active_outputs;
    expected_active_outputs.internal = 2;
    expected_active_outputs.external = 3;

    fake_screen->notify_active_outputs(expected_active_outputs);

    auto message = client.request_active_outputs_property().get();

    DBusMessageIter iter;
    dbus_message_iter_init(message, &iter);

    DBusMessageIter iter_variant;
    dbus_message_iter_recurse(&iter, &iter_variant);
    DBusMessageIter iter_values;
    dbus_message_iter_recurse(&iter_variant, &iter_values);

    usc::ActiveOutputs active_outputs{-1, -1};

    dbus_message_iter_get_basic(&iter_values, &active_outputs.internal);
    dbus_message_iter_next(&iter_values);
    dbus_message_iter_get_basic(&iter_values, &active_outputs.external);

    EXPECT_THAT(active_outputs, Eq(expected_active_outputs));
}

TEST_F(AUnityDisplayService, returns_all_properties)
{
    using namespace testing;

    usc::ActiveOutputs expected_active_outputs;
    expected_active_outputs.internal = 2;
    expected_active_outputs.external = 3;

    fake_screen->notify_active_outputs(expected_active_outputs);

    auto message = client.request_all_properties().get();

    DBusMessageIter iter;
    dbus_message_iter_init(message, &iter);
    DBusMessageIter iter_properties;
    dbus_message_iter_recurse(&iter, &iter_properties);
    DBusMessageIter iter_property;
    dbus_message_iter_recurse(&iter_properties, &iter_property);

    char const* property_name{""};
    usc::ActiveOutputs active_outputs{-1, -1};

    dbus_message_iter_get_basic(&iter_property, &property_name);

    dbus_message_iter_next(&iter_property);
    DBusMessageIter iter_variant;
    DBusMessageIter iter_values;
    dbus_message_iter_recurse(&iter_property, &iter_variant);
    dbus_message_iter_recurse(&iter_variant, &iter_values);

    dbus_message_iter_get_basic(&iter_values, &active_outputs.internal);
    dbus_message_iter_next(&iter_values);
    dbus_message_iter_get_basic(&iter_values, &active_outputs.external);

    EXPECT_THAT(property_name, StrEq("ActiveOutputs"));
    EXPECT_THAT(active_outputs, Eq(expected_active_outputs));
}

TEST_F(AUnityDisplayService, returns_error_reply_for_unsupported_method)
{
    using namespace testing;

    auto reply = client.request_invalid_method();
    auto reply_msg = reply.get();

    EXPECT_THAT(dbus_message_get_type(reply_msg), Eq(DBUS_MESSAGE_TYPE_ERROR));
    EXPECT_THAT(dbus_message_get_error_name(reply_msg), StrEq(DBUS_ERROR_FAILED));
}
