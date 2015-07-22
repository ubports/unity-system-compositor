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

#include "src/unity_screen_service.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_message_handle.h"
#include "src/screen.h"
#include "src/power_state_change_reason.h"
#include "src/unity_screen_service_introspection.h"
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

struct MockScreen : usc::Screen
{
    MOCK_METHOD1(enable_inactivity_timers, void(bool enable));
    MOCK_METHOD0(keep_display_on_temporarily, void());

    MOCK_METHOD0(get_screen_power_mode, MirPowerMode());
    MOCK_METHOD2(set_screen_power_mode, void(MirPowerMode mode, PowerStateChangeReason reason));
    MOCK_METHOD1(keep_display_on, void(bool on));
    MOCK_METHOD1(set_brightness, void(int brightness));
    MOCK_METHOD1(enable_auto_brightness, void(bool enable));
    MOCK_METHOD2(set_inactivity_timeouts, void(int power_off_timeout, int dimmer_timeout));

    MOCK_METHOD1(set_touch_visualization_enabled, void(bool enabled));

    void register_power_state_change_handler(
        usc::PowerStateChangeHandler const& handler)
    {
        power_state_change_handler = handler;
    }

    usc::PowerStateChangeHandler power_state_change_handler;
};

class UnityScreenDBusClient : public ut::DBusClient
{
public:
    UnityScreenDBusClient(std::string const& address)
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

    ut::DBusAsyncReplyString request_introspection()
    {
        return invoke_with_reply<ut::DBusAsyncReplyString>(
            "org.freedesktop.DBus.Introspectable", "Introspect",
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_set_user_brightness(int32_t brightness)
    {
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_screen_interface, "setUserBrightness",
            DBUS_TYPE_INT32, &brightness,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_user_auto_brightness_enable(bool enabled)
    {
        dbus_bool_t const e = enabled ? TRUE : FALSE;
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_screen_interface, "userAutobrightnessEnable",
            DBUS_TYPE_BOOLEAN, &e,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_set_inactivity_timeouts(
        int32_t poweroff_timeout, int32_t dimmer_timeout)
    {
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_screen_interface, "setInactivityTimeouts",
            DBUS_TYPE_INT32, &poweroff_timeout,
            DBUS_TYPE_INT32, &dimmer_timeout,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_set_touch_visualization_enabled(bool enabled)
    {
        dbus_bool_t const e = enabled ? TRUE : FALSE;
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_screen_interface, "setTouchVisualizationEnabled",
            DBUS_TYPE_BOOLEAN, &e,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyBool request_set_screen_power_mode(
        std::string const& mode, int reason)
    {
        auto mode_cstr = mode.c_str();
        return invoke_with_reply<ut::DBusAsyncReplyBool>(
            unity_screen_interface, "setScreenPowerMode",
            DBUS_TYPE_STRING, &mode_cstr,
            DBUS_TYPE_INT32, &reason,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyInt request_keep_display_on()
    {
        return invoke_with_reply<ut::DBusAsyncReplyInt>(
            unity_screen_interface, "keepDisplayOn",
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReplyVoid request_remove_display_on_request(int id)
    {
        return invoke_with_reply<ut::DBusAsyncReplyVoid>(
            unity_screen_interface, "removeDisplayOnRequest",
            DBUS_TYPE_INT32, &id,
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReply request_invalid_method()
    {
        return invoke_with_reply<ut::DBusAsyncReply>(
            unity_screen_interface, "invalidMethod",
            DBUS_TYPE_INVALID);
    }

    ut::DBusAsyncReply request_method_with_invalid_arguments()
    {
        char const* const str = "abcd";
        return invoke_with_reply<ut::DBusAsyncReply>(
            unity_screen_interface, "setUserBrightness",
            DBUS_TYPE_STRING, &str,
            DBUS_TYPE_INVALID);
    }

    usc::DBusMessageHandle listen_for_power_state_change_signal()
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

    char const* const unity_screen_interface = "com.canonical.Unity.Screen";
};

struct AUnityScreenService : testing::Test
{
    std::chrono::seconds const default_timeout{3};
    ut::DBusBus bus;

    std::shared_ptr<MockScreen> const mock_screen =
        std::make_shared<testing::NiceMock<MockScreen>>();
    UnityScreenDBusClient client{bus.address()};
    usc::UnityScreenService service{bus.address(), mock_screen};
};

}

TEST_F(AUnityScreenService, replies_to_introspection_request)
{
    using namespace testing;

    auto reply = client.request_introspection();
    EXPECT_THAT(reply.get(), Eq(unity_screen_service_introspection));
}

TEST_F(AUnityScreenService, forwards_set_user_brightness_request)
{
    int const brightness = 10;

    EXPECT_CALL(*mock_screen, set_brightness(brightness));

    client.request_set_user_brightness(brightness);
}

TEST_F(AUnityScreenService, forwards_user_auto_brightness_enable_request)
{
    bool const enable = true;

    EXPECT_CALL(*mock_screen, enable_auto_brightness(enable));

    client.request_user_auto_brightness_enable(enable);
}

TEST_F(AUnityScreenService, forwards_set_inactivity_timeouts_request)
{
    int const poweroff_timeout = 1000;
    int const dimmer_timeout = 500;

    EXPECT_CALL(*mock_screen, set_inactivity_timeouts(poweroff_timeout, dimmer_timeout));

    client.request_set_inactivity_timeouts(poweroff_timeout, dimmer_timeout);
}

TEST_F(AUnityScreenService, forwards_set_touch_visualization_enabled_request)
{
    bool const enabled = true;

    EXPECT_CALL(*mock_screen, set_touch_visualization_enabled(enabled));

    client.request_set_touch_visualization_enabled(enabled);
}

TEST_F(AUnityScreenService, forwards_set_screen_power_mode_request)
{
    auto const mode = MirPowerMode::mir_power_mode_standby;
    std::string const mode_str = "standby";
    auto const reason = PowerStateChangeReason::proximity;
    auto const reason_int = static_cast<int>(reason);

    EXPECT_CALL(*mock_screen, set_screen_power_mode(mode, reason));

    client.request_set_screen_power_mode(mode_str, reason_int);
}

TEST_F(AUnityScreenService, replies_to_set_screen_power_mode_request)
{
    using namespace testing;

    std::string const mode_str = "standby";
    auto const reason_int = static_cast<int>(PowerStateChangeReason::proximity);

    auto reply = client.request_set_screen_power_mode(mode_str, reason_int);

    EXPECT_THAT(reply.get(), Eq(true));
}

TEST_F(AUnityScreenService, forwards_keep_display_on_request)
{
    EXPECT_CALL(*mock_screen, keep_display_on(true));

    client.request_keep_display_on();
}

TEST_F(AUnityScreenService, replies_with_different_ids_to_keep_display_on_requests)
{
    using namespace testing;

    auto reply1 = client.request_keep_display_on();
    auto reply2 = client.request_keep_display_on();

    auto const id1 = reply1.get();
    auto const id2 = reply2.get();

    EXPECT_THAT(id1, Ne(id2));
}

TEST_F(AUnityScreenService, disables_keep_display_on_when_single_request_is_removed)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(*mock_screen, keep_display_on(true));
    EXPECT_CALL(*mock_screen, keep_display_on(false));

    auto reply1 = client.request_keep_display_on();
    client.request_remove_display_on_request(reply1.get());
}

TEST_F(AUnityScreenService, disables_keep_display_on_when_all_requests_are_removed)
{
    using namespace testing;

    EXPECT_CALL(*mock_screen, keep_display_on(true)).Times(3);
    EXPECT_CALL(*mock_screen, keep_display_on(false)).Times(0);

    auto reply1 = client.request_keep_display_on();
    auto reply2 = client.request_keep_display_on();
    auto reply3 = client.request_keep_display_on();

    client.request_remove_display_on_request(reply1.get());
    client.request_remove_display_on_request(reply2.get());
    auto id3 = reply3.get();

    // Display should still be kept on at this point
    Mock::VerifyAndClearExpectations(mock_screen.get());

    // keep_display_on should be disable only when the last request is removed
    EXPECT_CALL(*mock_screen, keep_display_on(false));

    client.request_remove_display_on_request(id3);
}

TEST_F(AUnityScreenService, disables_keep_display_on_when_single_client_disconnects)
{
    ut::WaitCondition request_processed;

    EXPECT_CALL(*mock_screen, keep_display_on(true)).Times(3);
    EXPECT_CALL(*mock_screen, keep_display_on(false))
        .WillOnce(WakeUp(&request_processed));

    client.request_keep_display_on();
    client.request_keep_display_on();
    client.request_keep_display_on();

    client.disconnect();

    request_processed.wait_for(default_timeout);
    EXPECT_TRUE(request_processed.woken());
}

TEST_F(AUnityScreenService, disables_keep_display_on_when_all_clients_disconnect_or_remove_requests)
{
    using namespace testing;

    UnityScreenDBusClient other_client{bus.address()};

    EXPECT_CALL(*mock_screen, keep_display_on(true)).Times(4);
    EXPECT_CALL(*mock_screen, keep_display_on(false)).Times(0);

    auto reply1 = client.request_keep_display_on();
    auto reply2 = client.request_keep_display_on();
    other_client.request_keep_display_on();
    other_client.request_keep_display_on();

    other_client.disconnect();
    client.request_remove_display_on_request(reply1.get());
    auto id2 = reply2.get();

    // Display should still be kept on at this point
    Mock::VerifyAndClearExpectations(mock_screen.get());

    // keep_display_on should be disabled only when the last request is removed
    ut::WaitCondition request_processed;
    EXPECT_CALL(*mock_screen, keep_display_on(false))
        .WillOnce(WakeUp(&request_processed));

    client.request_remove_display_on_request(id2);

    request_processed.wait_for(default_timeout);
    EXPECT_TRUE(request_processed.woken());
}

TEST_F(AUnityScreenService, ignores_invalid_display_on_removal_request)
{
    ut::WaitCondition request_processed;

    int32_t const invalid_id{-1};
    EXPECT_CALL(*mock_screen, keep_display_on(false)).Times(0);

    client.request_remove_display_on_request(invalid_id);
    client.disconnect();

    // Allow some time for dbus calls to reach UnityScreenService
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(AUnityScreenService, ignores_disconnects_from_clients_without_display_on_request)
{
    ut::WaitCondition request_processed;

    EXPECT_CALL(*mock_screen, keep_display_on(false)).Times(0);

    client.disconnect();

    // Allow some time for disconnect notification to reach UnityScreenService
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(AUnityScreenService, emits_power_state_change_signal)
{
    using namespace testing;

    auto async_message = std::async(std::launch::async,
        [&] { return client.listen_for_power_state_change_signal(); });

    mock_screen->power_state_change_handler(
        MirPowerMode::mir_power_mode_off, PowerStateChangeReason::power_key);

    auto message = async_message.get();

    int32_t state{-1};
    int32_t reason{-1};
    dbus_message_get_args(message, nullptr,
        DBUS_TYPE_INT32, &state,
        DBUS_TYPE_INT32, &reason,
        DBUS_TYPE_INVALID);

    int32_t const off_state{0};

    EXPECT_THAT(state, Eq(off_state));
    EXPECT_THAT(reason, Eq(static_cast<int32_t>(PowerStateChangeReason::power_key)));
}

TEST_F(AUnityScreenService, returns_error_reply_for_unsupported_method)
{
    using namespace testing;

    auto reply = client.request_invalid_method();
    auto reply_msg = reply.get();

    EXPECT_THAT(dbus_message_get_type(reply_msg), Eq(DBUS_MESSAGE_TYPE_ERROR));
    EXPECT_THAT(dbus_message_get_error_name(reply_msg), StrEq(DBUS_ERROR_FAILED));
}

TEST_F(AUnityScreenService, returns_error_reply_for_method_with_invalid_arguments)
{
    using namespace testing;

    auto reply = client.request_method_with_invalid_arguments();
    auto reply_msg = reply.get();

    EXPECT_THAT(dbus_message_get_type(reply_msg), Eq(DBUS_MESSAGE_TYPE_ERROR));
    EXPECT_THAT(dbus_message_get_error_name(reply_msg), StrEq(DBUS_ERROR_FAILED));
}
