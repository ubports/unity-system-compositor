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

#include "src/powerd_mediator.h"
#include "src/dbus_connection_handle.h"
#include "src/dbus_message_handle.h"
#include "src/dbus_event_loop.h"
#include "wait_condition.h"
#include "dbus_bus.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace ut = usc::test;

namespace
{

class PowerdService
{
public:
    enum class StartNow { no, yes };

    PowerdService(
        std::string const& address,
        dbus_bool_t auto_brightness_supported,
        StartNow start_now)
        : auto_brightness_supported{auto_brightness_supported},
          connection{address.c_str()},
          dbus_event_loop{connection}
    {
        if (start_now == StartNow::yes)
            start();
    }

    ~PowerdService()
    {
        dbus_event_loop.stop();
        if (dbus_loop_thread.joinable())
            dbus_loop_thread.join();
    }

    void start()
    {
        ON_CALL(*this, dbus_setUserBrightness(normal_brightness))
            .WillByDefault(WakeUp(&initial_setup_done));

        connection.request_name(powerd_service_name);
        connection.add_filter(handle_dbus_message_thunk, this);

        std::promise<void> event_loop_started;
        auto event_loop_started_future = event_loop_started.get_future();

        dbus_loop_thread = std::thread(
            [this,&event_loop_started]
            {
                dbus_event_loop.run(event_loop_started);
            });

        event_loop_started_future.wait();
    }

    void wait_for_initial_setup()
    {
        initial_setup_done.wait_for(std::chrono::seconds{5});
    }

    MOCK_METHOD1(dbus_setUserBrightness, void(int brightness));
    MOCK_METHOD1(dbus_userAutobrightnessEnable, void(dbus_bool_t enable));
    MOCK_METHOD2(dbus_requestSysState, std::string(char const* name, int state));
    MOCK_METHOD1(dbus_clearSysState, void(char const* cookie));
    MOCK_METHOD1(dbus_enableProximityHandling, void(char const* name));
    MOCK_METHOD1(dbus_disableProximityHandling, void(char const* name));

    int32_t const dim_brightness = 13;
    int32_t const min_brightness = 5;
    int32_t const max_brightness = 42;
    int32_t const normal_brightness = 24;
    dbus_bool_t const auto_brightness_supported;
    const char* const default_cookie = "monster";

    void emit_SysPowerStateChange(int32_t state)
    {
        usc::DBusMessageHandle signal{
            dbus_message_new_signal(
                powerd_service_path,
                powerd_service_interface,
                "SysPowerStateChange"),
            DBUS_TYPE_INT32, &state,
            DBUS_TYPE_INVALID};

        dbus_connection_send(connection, signal, nullptr);
    }

private:
    static ::DBusHandlerResult handle_dbus_message_thunk(
        ::DBusConnection* connection, DBusMessage* message, void* user_data)
    {
        auto const powerd_service = static_cast<PowerdService*>(user_data);
        return powerd_service->handle_dbus_message(connection, message, user_data);
    }

    ::DBusHandlerResult handle_dbus_message(
        ::DBusConnection* connection, DBusMessage* message, void* user_data)
    {
        if (dbus_message_is_method_call(message, powerd_service_name, "getBrightnessParams"))
        {
            usc::DBusMessageHandle reply{dbus_message_new_method_return(message)};
            DBusMessageIter iter;
            dbus_message_iter_init_append(reply, &iter);

            DBusMessageIter iter_struct;
            dbus_message_iter_open_container(&iter, DBUS_TYPE_STRUCT, nullptr, &iter_struct);
            dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &dim_brightness);
            dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &min_brightness);
            dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &max_brightness);
            dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_INT32, &normal_brightness);
            dbus_message_iter_append_basic(&iter_struct, DBUS_TYPE_BOOLEAN, &auto_brightness_supported);
            dbus_message_iter_close_container(&iter, &iter_struct);

            dbus_connection_send(connection, reply, nullptr);
        }
        else if (dbus_message_is_method_call(message, powerd_service_name, "setUserBrightness"))
        {
            int32_t brightness{0};
            dbus_message_get_args(
                message, nullptr, DBUS_TYPE_INT32, &brightness, DBUS_TYPE_INVALID);
            dbus_setUserBrightness(brightness);

            usc::DBusMessageHandle reply{dbus_message_new_method_return(message)};
            dbus_connection_send(connection, reply, nullptr);
        }
        else if (dbus_message_is_method_call(message, powerd_service_name, "userAutobrightnessEnable"))
        {
            dbus_bool_t enable{FALSE};
            dbus_message_get_args(
                message, nullptr, DBUS_TYPE_BOOLEAN, &enable, DBUS_TYPE_INVALID);
            dbus_userAutobrightnessEnable(enable);

            usc::DBusMessageHandle reply{dbus_message_new_method_return(message)};
            dbus_connection_send(connection, reply, nullptr);
        }
        else if (dbus_message_is_method_call(message, powerd_service_name, "requestSysState"))
        {
            char const* name{nullptr};
            int32_t state{-1};
            dbus_message_get_args(
                message, nullptr,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INT32, &state,
                DBUS_TYPE_INVALID);

            auto cookie = dbus_requestSysState(name, state);
            if (cookie.empty())
                cookie = default_cookie;
            auto cookie_cstr = cookie.c_str();

            usc::DBusMessageHandle reply{
                dbus_message_new_method_return(message),
                DBUS_TYPE_STRING, &cookie_cstr,
                DBUS_TYPE_INVALID};

            dbus_connection_send(connection, reply, nullptr);

            emit_SysPowerStateChange(state);
        }
        else if (dbus_message_is_method_call(message, powerd_service_name, "clearSysState"))
        {
            char const* cookie{nullptr};
            dbus_message_get_args(
                message, nullptr,
                DBUS_TYPE_STRING, &cookie,
                DBUS_TYPE_INVALID);

            dbus_clearSysState(cookie);

            usc::DBusMessageHandle reply{dbus_message_new_method_return(message)};
            dbus_connection_send(connection, reply, nullptr);

            emit_SysPowerStateChange(0);
        }
        else if (dbus_message_is_method_call(message, powerd_service_name, "enableProximityHandling"))
        {
            char const* name{nullptr};
            dbus_message_get_args(
                message, nullptr,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID);

            dbus_enableProximityHandling(name);

            usc::DBusMessageHandle reply{dbus_message_new_method_return(message)};
            dbus_connection_send(connection, reply, nullptr);
        }
        else if (dbus_message_is_method_call(message, powerd_service_name, "disableProximityHandling"))
        {
            char const* name{nullptr};
            dbus_message_get_args(
                message, nullptr,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_INVALID);

            dbus_disableProximityHandling(name);

            usc::DBusMessageHandle reply{dbus_message_new_method_return(message)};
            dbus_connection_send(connection, reply, nullptr);
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    char const* const powerd_service_name = "com.canonical.powerd";
    char const* const powerd_service_path = "/com/canonical/powerd";
    char const* const powerd_service_interface = "com.canonical.powerd";

    ut::WaitCondition initial_setup_done;

    usc::DBusConnectionHandle connection;
    usc::DBusEventLoop dbus_event_loop;
    std::thread dbus_loop_thread;
};

struct APowerdMediatorBaseFixture : testing::Test
{
    std::chrono::seconds const default_timeout{3};

    APowerdMediatorBaseFixture(
        dbus_bool_t auto_brightness_supported,
        PowerdService::StartNow start_powerd_service_now)
        : powerd_service{bus.address(), auto_brightness_supported, start_powerd_service_now}
    {
    }

    ~APowerdMediatorBaseFixture()
    {
        expect_shutdown_requests();
    }

    void expect_shutdown_requests()
    {
        using namespace testing;

        Mock::VerifyAndClearExpectations(&powerd_service);

        EXPECT_CALL(powerd_service, dbus_setUserBrightness(0)).Times(AtMost(1));
        EXPECT_CALL(powerd_service, dbus_clearSysState(_)).Times(AtMost(1));
    }

    static dbus_bool_t const auto_brightness_is_supported{TRUE};
    static dbus_bool_t const auto_brightness_not_supported{FALSE};

    ut::DBusBus bus;
    testing::NiceMock<PowerdService> powerd_service;
    std::shared_ptr<usc::PowerdMediator> mediator{
        std::make_shared<usc::PowerdMediator>(bus.address())};
};

}

namespace
{

struct APowerdMediator : APowerdMediatorBaseFixture
{
    APowerdMediator()
        : APowerdMediatorBaseFixture{
            auto_brightness_is_supported, PowerdService::StartNow::yes}
    {
    }
};

}

TEST_F(APowerdMediator, gets_initial_brightness_values_from_service)
{
    using namespace testing;

    EXPECT_THAT(mediator->min_brightness(), Eq(powerd_service.min_brightness));
    EXPECT_THAT(mediator->max_brightness(), Eq(powerd_service.max_brightness));
    EXPECT_THAT(mediator->auto_brightness_supported(),
                Eq(powerd_service.auto_brightness_supported == TRUE));
}

TEST_F(APowerdMediator, sets_dim_brightness_for_dim_backlight)
{
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));

    mediator->set_dim_backlight();
}

TEST_F(APowerdMediator, does_not_set_dim_brightness_if_not_needed)
{
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));

    mediator->set_dim_backlight();
    mediator->set_dim_backlight();
}

TEST_F(APowerdMediator, sets_normal_brightness_for_normal_backlight)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.normal_brightness));

    // Start-up brightness is normal, so we need to change to some other value
    // first before resetting normal brightness.
    mediator->set_dim_backlight();
    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, does_not_set_normal_brightness_if_not_needed)
{
    using namespace testing;

    EXPECT_CALL(powerd_service, dbus_setUserBrightness(_)).Times(0);

    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, sets_zero_brightness_for_off_backlight)
{
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(0));

    mediator->turn_off_backlight();
}

TEST_F(APowerdMediator, uses_user_specified_brightness_values_if_set)
{
    using namespace testing;

    auto const dim_brightness = powerd_service.dim_brightness + 1;
    auto const normal_brightness = powerd_service.normal_brightness + 1;

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(dim_brightness));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(normal_brightness));

    mediator->change_backlight_values(dim_brightness, normal_brightness);
    mediator->set_dim_backlight();
    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, ignores_out_of_range_user_specified_brightness_values)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.normal_brightness));

    mediator->change_backlight_values(mediator->max_brightness() + 1,
                                     mediator->max_brightness() + 1);
    mediator->change_backlight_values(mediator->min_brightness() - 1,
                                     mediator->min_brightness() - 1);
    mediator->set_dim_backlight();
    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, enables_auto_brightness_on_request)
{
    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE));

    mediator->enable_auto_brightness(true);
}

TEST_F(APowerdMediator, sets_normal_brightness_when_disabling_auto_brightness)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.normal_brightness));

    mediator->enable_auto_brightness(true);
    mediator->enable_auto_brightness(false);
}

TEST_F(APowerdMediator, retains_auto_brightness_mode_when_setting_normal_backlight)
{
    using namespace testing;

    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(_)).Times(0);

    mediator->enable_auto_brightness(true);
    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, retains_auto_brightness_mode_when_setting_normal_backlight_after_off)
{
    using namespace testing;

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(0));
    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE));

    mediator->enable_auto_brightness(true);
    mediator->turn_off_backlight();
    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, sets_explicit_brightness_value)
{
    ut::WaitCondition request_processed;

    int const brightness{30};
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(brightness));

    mediator->set_brightness(brightness);
}

TEST_F(APowerdMediator, changes_normal_brightness_when_explicitly_setting_brightness)
{
    using namespace testing;

    int const brightness{30};

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(brightness));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(brightness));

    mediator->set_brightness(brightness);
    mediator->set_dim_backlight();
    // Normal backlight should use value set by explicit set_brightness() call
    mediator->set_normal_backlight();
}

TEST_F(APowerdMediator, retains_auto_brightness_mode_when_explicitly_setting_brightness)
{
    using namespace testing;

    int const brightness{30};

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(brightness)).Times(0);

    mediator->enable_auto_brightness(true);
    // set_brightness() shouldn't change mode since auto brightness is enabled
    // (i.e. it shouldn't contact powerd)
    mediator->set_brightness(brightness);
}

TEST_F(APowerdMediator, ignores_disable_suspend_request_if_suspend_already_disabled)
{
    using namespace testing;

    EXPECT_CALL(powerd_service, dbus_requestSysState(_, _)).Times(0);

    mediator->disable_suspend();
}

TEST_F(APowerdMediator, waits_for_state_change_when_disabling_suspend)
{
    using namespace testing;

    mediator->allow_suspend();
    while (!mediator->is_system_suspended())
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

    mediator->disable_suspend();
    EXPECT_TRUE(!mediator->is_system_suspended());
}

TEST_F(APowerdMediator, uses_correct_cookie_when_allowing_suspend)
{
    using namespace testing;

    EXPECT_CALL(powerd_service, dbus_clearSysState(StrEq(powerd_service.default_cookie)));

    mediator->allow_suspend();
}

TEST_F(APowerdMediator, enables_and_disables_proximity_handling)
{
    using namespace testing;
    auto const proximity_name = "unity-system-compositor";

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_enableProximityHandling(StrEq(proximity_name)));
    EXPECT_CALL(powerd_service, dbus_disableProximityHandling(StrEq(proximity_name)));

    mediator->enable_proximity(true);
    mediator->enable_proximity(false);
}

TEST_F(APowerdMediator, ignores_requests_for_redundant_proximity_handling)
{
    using namespace testing;

    auto const proximity_name = "unity-system-compositor";

    InSequence s;
    EXPECT_CALL(powerd_service, dbus_enableProximityHandling(StrEq(proximity_name)));
    EXPECT_CALL(powerd_service, dbus_disableProximityHandling(StrEq(proximity_name)));

    // Should be ignore, proximity already disabled
    mediator->enable_proximity(false);

    // Should be handled only once
    mediator->enable_proximity(true);
    mediator->enable_proximity(true);

    // Should be handled only once
    mediator->enable_proximity(false);
    mediator->enable_proximity(false);
}

namespace
{

struct APowerdMediatorWithoutAutoBrightnessSupport : APowerdMediatorBaseFixture
{
    APowerdMediatorWithoutAutoBrightnessSupport()
        : APowerdMediatorBaseFixture{
            auto_brightness_not_supported, PowerdService::StartNow::yes}
    {
    }
};

}

TEST_F(APowerdMediatorWithoutAutoBrightnessSupport,
       sets_normal_brightness_for_enable_auto_brightness)
{
    using namespace testing;

    InSequence s;
    // (1)
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));
    // (2) Since auto brightness is not supported, when trying to enable it we should see
    // we should just get normal brightness.
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.normal_brightness));

    mediator->set_dim_backlight(); // (1)
    mediator->enable_auto_brightness(true); // (2)
}

TEST_F(APowerdMediatorWithoutAutoBrightnessSupport,
       sets_normal_brightness_for_normal_backlight_when_auto_brightness_not_supported)
{
    using namespace testing;

    InSequence s;
    // (1)
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.dim_brightness));
    // (2) Since auto brightness is not supported, when setting normal backlight we should
    // get normal brightness, even though the user has previously tried to enable
    // auto brightness (call marked (*) below).
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.normal_brightness));

    // This call should have no effect, since we are already in normal backlight state
    mediator->enable_auto_brightness(true); // (*)
    mediator->set_dim_backlight(); // (1)
    mediator->set_normal_backlight(); // (2)
}

namespace
{

struct APowerdMediatorWithoutInitialService : APowerdMediatorBaseFixture
{
    APowerdMediatorWithoutInitialService()
        : APowerdMediatorBaseFixture{
            auto_brightness_is_supported, PowerdService::StartNow::no}
    {
    }
};

}

TEST_F(APowerdMediatorWithoutInitialService, has_sensible_brightness_values)
{
    using namespace testing;

    EXPECT_THAT(mediator->min_brightness(), Ge(0));
    EXPECT_THAT(mediator->max_brightness(), Ge(mediator->min_brightness()));
    EXPECT_THAT(mediator->auto_brightness_supported(), Eq(false));
}

TEST_F(APowerdMediatorWithoutInitialService,
       updates_brightness_values_when_service_connects)
{
    using namespace testing;

    powerd_service.start();
    powerd_service.wait_for_initial_setup();

    EXPECT_THAT(mediator->min_brightness(), Eq(powerd_service.min_brightness));
    EXPECT_THAT(mediator->max_brightness(), Eq(powerd_service.max_brightness));
    EXPECT_THAT(mediator->auto_brightness_supported(),
                Eq(powerd_service.auto_brightness_supported == TRUE));
}

TEST_F(APowerdMediatorWithoutInitialService,
       disables_suspend_if_suspend_not_allowed_when_service_connects)
{
    using namespace testing;

    int const sys_state_active{1};

    ut::WaitCondition request_processed;

    EXPECT_CALL(powerd_service, dbus_requestSysState(_, sys_state_active))
        .WillOnce(DoAll(WakeUp(&request_processed), Return("")));

    powerd_service.start();
    powerd_service.wait_for_initial_setup();

    request_processed.wait_for(default_timeout);
    EXPECT_TRUE(request_processed.woken());

}

TEST_F(APowerdMediatorWithoutInitialService,
       does_not_disable_suspend_if_suspend_is_allowed_when_service_connects)
{
    using namespace testing;

    mediator->allow_suspend();

    ut::WaitCondition request_processed;

    EXPECT_CALL(powerd_service, dbus_requestSysState(_, _)).Times(0);

    powerd_service.start();
    powerd_service.wait_for_initial_setup();
}

TEST_F(APowerdMediatorWithoutInitialService,
       reenables_auto_brightness_if_it_is_enabled_when_service_connects)
{
    using namespace testing;

    // Connect powerd service, enable auto brightness, disconnect service
    {
        NiceMock<PowerdService> another_powerd_service{
            bus.address(), TRUE, PowerdService::StartNow::yes};
        another_powerd_service.wait_for_initial_setup();

        EXPECT_CALL(another_powerd_service, dbus_userAutobrightnessEnable(TRUE));

        mediator->enable_auto_brightness(true);
    }

    // Connect service again
    ut::WaitCondition request_processed;

    EXPECT_CALL(powerd_service, dbus_userAutobrightnessEnable(TRUE))
        .WillOnce(WakeUp(&request_processed));

    powerd_service.start();
    // We don't wait for initial default setup, since it won't happen
    // in this case.

    request_processed.wait_for(default_timeout);
    EXPECT_TRUE(request_processed.woken());
}

TEST(APowerdMediatorAtStartup, turns_on_backlight_and_disables_suspend)
{
    using namespace testing;

    const char* const name{"com.canonical.Unity.Screen"};
    int const sys_state_active{1};
    dbus_bool_t const auto_brightness_supported{TRUE};

    ut::DBusBus bus;
    NiceMock<PowerdService> powerd_service{
        bus.address(), auto_brightness_supported, PowerdService::StartNow::yes};

    EXPECT_CALL(powerd_service, dbus_setUserBrightness(powerd_service.normal_brightness));
    EXPECT_CALL(powerd_service, dbus_requestSysState(StrEq(name), sys_state_active));

    usc::PowerdMediator mediator{bus.address()};
    powerd_service.wait_for_initial_setup();

    Mock::VerifyAndClearExpectations(&powerd_service);

    EXPECT_CALL(powerd_service, dbus_clearSysState(_)).Times(AtMost(1));
    EXPECT_CALL(powerd_service, dbus_setUserBrightness(0)).Times(AtMost(1));
}

TEST(APowerdMediatorAtStartup, disables_proximity_handling)
{
    using namespace testing;

    dbus_bool_t const auto_brightness_supported{TRUE};

    ut::DBusBus bus;
    NiceMock<PowerdService> powerd_service{
        bus.address(), auto_brightness_supported, PowerdService::StartNow::yes};

    EXPECT_CALL(powerd_service, dbus_disableProximityHandling(_));

    usc::PowerdMediator mediator{bus.address()};
    powerd_service.wait_for_initial_setup();

    Mock::VerifyAndClearExpectations(&powerd_service);
}

TEST(APowerdMediatorAtShutdown, turns_off_backlight)
{
    using namespace testing;

    ut::DBusBus bus;
    dbus_bool_t const auto_brightness_supported{TRUE};
    NiceMock<PowerdService> powerd_service{
        bus.address(), auto_brightness_supported, PowerdService::StartNow::yes};

    usc::PowerdMediator mediator{bus.address()};
    powerd_service.wait_for_initial_setup();

    Mock::VerifyAndClearExpectations(&powerd_service);

    EXPECT_CALL(powerd_service, dbus_setUserBrightness(0));
}
