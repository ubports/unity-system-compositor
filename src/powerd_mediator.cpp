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
 */

#include "powerd_mediator.h"
#include "thread_name.h"
#include "scoped_dbus_error.h"

#include "dbus_message_handle.h"

#include <future>
#include <cstdint>

namespace
{

char const* const powerd_service_name = "com.canonical.powerd";
char const* const powerd_service_interface = "com.canonical.powerd";
char const* const powerd_service_path = "/com/canonical/powerd";

usc::DBusMessageHandle make_powerd_method_call_message(
    char const* method, int first_arg_type, va_list args)
{
    return usc::DBusMessageHandle{
        dbus_message_new_method_call(
            powerd_service_name,
            powerd_service_path,
            powerd_service_interface,
            method),
        first_arg_type, args};
}

}

usc::PowerdMediator::PowerdMediator(std::string const& bus_addr)
    : connection{bus_addr.c_str()},
      dbus_event_loop{connection},
      pending_suspend_block_request{false},
      dim_brightness_{10},
      min_brightness_{0},
      max_brightness_{102},
      normal_brightness_{102},
      current_brightness{0},
      backlight_state{BacklightState::normal},
      auto_brightness_supported_{false},
      auto_brightness_requested{false},
      proximity_enabled{false},
      sys_state{SysState::unknown}
{
    connection.add_match(
        "type='signal',"
        "sender='com.canonical.powerd',"
        "interface='com.canonical.powerd',"
        "member='SysPowerStateChange'");
    connection.add_match(
        "type='signal',"
        "sender='org.freedesktop.DBus',"
        "interface='org.freedesktop.DBus',"
        "member='NameOwnerChanged'");
    connection.add_filter(handle_dbus_message_thunk, this);

    std::promise<void> event_loop_started;
    auto event_loop_started_future = event_loop_started.get_future();

    dbus_loop_thread = std::thread(
        [this,&event_loop_started]
        {
            usc::set_thread_name("USC/DBusPowerd");
            dbus_event_loop.run(event_loop_started);
        });

    event_loop_started_future.wait();

    init_powerd_state(ForceDisableSuspend::yes);
}

usc::PowerdMediator::~PowerdMediator()
{
    turn_off_backlight();

    dbus_event_loop.stop();
    dbus_loop_thread.join();
}

void usc::PowerdMediator::set_dim_backlight()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    change_backlight_state(BacklightState::dim, ForceBacklightState::no);
}

void usc::PowerdMediator::set_normal_backlight()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    if (auto_brightness_supported_ && auto_brightness_requested)
        change_backlight_state(BacklightState::automatic, ForceBacklightState::no);
    else
        change_backlight_state(BacklightState::normal, ForceBacklightState::no);
}

void usc::PowerdMediator::turn_off_backlight()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    change_backlight_state(BacklightState::off, ForceBacklightState::no);
}

void usc::PowerdMediator::change_backlight_values(int dim, int normal)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    if (is_valid_brightness(dim))
        dim_brightness_ = dim;
    if (is_valid_brightness(normal))
        normal_brightness_ = normal;
}

void usc::PowerdMediator::allow_suspend()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    if (!suspend_block_cookie.empty())
    {
        auto const cstr = suspend_block_cookie.c_str();
        invoke_with_reply("clearSysState",
               DBUS_TYPE_STRING, &cstr,
               DBUS_TYPE_INVALID);

        suspend_block_cookie.clear();
    }

    pending_suspend_block_request = false;
}

void usc::PowerdMediator::disable_suspend()
{
    std::unique_lock<decltype(mutex)> lock{mutex};

    if (request_suspend_block())
    {
        lock.unlock();
        wait_for_sys_state(SysState::active);
    }
}

void usc::PowerdMediator::enable_auto_brightness(bool enable)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    auto_brightness_requested = enable;

    if (auto_brightness_supported_ && enable)
        change_backlight_state(BacklightState::automatic, ForceBacklightState::no);
    else
        change_backlight_state(BacklightState::normal, ForceBacklightState::no);
}

bool usc::PowerdMediator::auto_brightness_supported()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    return auto_brightness_supported_;
}

void usc::PowerdMediator::set_brightness(int brightness)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    normal_brightness_ = brightness;
    if (backlight_state != BacklightState::automatic)
        change_backlight_state(BacklightState::normal, ForceBacklightState::yes);
}

int usc::PowerdMediator::min_brightness()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    return min_brightness_;
}

int usc::PowerdMediator::max_brightness()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    return max_brightness_;
}

void usc::PowerdMediator::enable_proximity(bool enable)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    enable_proximity_l(enable, ForceProximity::no);
}

bool usc::PowerdMediator::is_system_suspended()
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    return sys_state == SysState::suspend;
}

::DBusHandlerResult usc::PowerdMediator::handle_dbus_message_thunk(
    ::DBusConnection* connection, DBusMessage* message, void* user_data)
{
    auto const powerd_mediator = static_cast<PowerdMediator*>(user_data);

    return powerd_mediator->handle_dbus_message(connection, message, user_data);
}

::DBusHandlerResult usc::PowerdMediator::handle_dbus_message(
    ::DBusConnection* connection, ::DBusMessage* message, void* user_data)
{
    ScopedDBusError error;

    if (dbus_message_is_signal(message, powerd_service_interface, "SysPowerStateChange"))
    {
        int32_t state{-1};
        dbus_message_get_args(
            message, &error, DBUS_TYPE_INT32, &state, DBUS_TYPE_INVALID);

        if (!error)
        {
            update_sys_state(state == static_cast<int32_t>(SysState::suspend) ?
                             SysState::suspend : SysState::active);
        }
    }
    else if (dbus_message_is_signal(message, "org.freedesktop.DBus", "NameOwnerChanged"))
    {
        char const* name = "";
        char const* old_owner = "";
        char const* new_owner = "";

        dbus_message_get_args(
                message, &error,
                DBUS_TYPE_STRING, &name,
                DBUS_TYPE_STRING, &old_owner,
                DBUS_TYPE_STRING, &new_owner,
                DBUS_TYPE_INVALID);

        if (!error &&
            std::string{powerd_service_name} == name &&
            *old_owner == '\0' &&
            *new_owner != '\0')
        {
            init_powerd_state(ForceDisableSuspend::no);
        }
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

void usc::PowerdMediator::init_powerd_state(ForceDisableSuspend force_disable_suspend)
{
    std::lock_guard<decltype(mutex)> lock{mutex};

    init_brightness_params();

    /*
     * A suspend block request needs to be issued here on the following scenarios:
     * 1. powerd has restarted and PowerdMediator had already issued a request
     *    to the previous powerd instance
     * 2. When booting up the screen is assumed to be on and consequently we need to also issue
     *    a system suspend block request.
     * 3. If powerd interface is not available yet, but the screen had been turned on
     *    then now is the time to issue the request
     */
    if (!suspend_block_cookie.empty() || pending_suspend_block_request ||
        force_disable_suspend == ForceDisableSuspend::yes)
    {
        // Clear the previous cookie as powerd has restarted anyway
        suspend_block_cookie.clear();
        if (request_suspend_block())
        {
            /*
             * If powerd is already up it may already be in the active state
             * and the SysPowerStateChange signal could have already been broadcasted
             * before we got a chance to register a listener for it.
             * We will assume that if the active request succeeds that the system state
             * will become active at some point in the future - this is only a workaround
             * for the lack of a system state query api in powerd
             */
            update_sys_state(SysState::active);
        }
    }

    // Powerd may have restarted, re-apply backlight settings
    change_backlight_state(backlight_state, ForceBacklightState::yes);

    enable_proximity_l(false, ForceProximity::yes);
}

void usc::PowerdMediator::init_brightness_params()
{
    auto reply = invoke_with_reply("getBrightnessParams", DBUS_TYPE_INVALID);

    if (reply)
    {
        DBusMessageIter msg_iter;
        dbus_message_iter_init(reply, &msg_iter);
        if (dbus_message_iter_get_arg_type(&msg_iter) != DBUS_TYPE_STRUCT)
            return;

        DBusMessageIter struct_iter;
        dbus_message_iter_recurse(&msg_iter, &struct_iter);

        int32_t dim, min, max, normal;
        dbus_bool_t auto_b;
        struct { int type; void* address; } args[]{
            { DBUS_TYPE_INT32, &dim },
            { DBUS_TYPE_INT32, &min },
            { DBUS_TYPE_INT32, &max },
            { DBUS_TYPE_INT32, &normal },
            { DBUS_TYPE_BOOLEAN, &auto_b }
        };

        for (auto const& arg : args)
        {
            if (dbus_message_iter_get_arg_type(&struct_iter) == arg.type)
                dbus_message_iter_get_basic(&struct_iter, arg.address);
            else
                return;
            dbus_message_iter_next(&struct_iter);
        }

        dim_brightness_ = dim;
        min_brightness_ = min;
        max_brightness_ = max;
        normal_brightness_ = normal;
        auto_brightness_supported_ = (auto_b == TRUE);
    }
}

void usc::PowerdMediator::change_backlight_state(
    BacklightState new_state,
    ForceBacklightState force_backlight_state)
{
    if (backlight_state == new_state &&
        force_backlight_state == ForceBacklightState::no)
    {
        return;
    }

    update_current_brightness_for_state(new_state);
    backlight_state = new_state;

    if (new_state == BacklightState::automatic)
    {
        dbus_bool_t const enable{TRUE};
        invoke_with_reply("userAutobrightnessEnable",
               DBUS_TYPE_BOOLEAN, &enable,
               DBUS_TYPE_INVALID);
    }
    else
    {
        int32_t const b{current_brightness};
        invoke_with_reply("setUserBrightness",
               DBUS_TYPE_INT32, &b,
               DBUS_TYPE_INVALID);
    }
}

void usc::PowerdMediator::enable_proximity_l(
    bool enable, ForceProximity force_proximity)
{
    if (proximity_enabled == enable && force_proximity == ForceProximity::no)
        return;

    auto const name = "unity-system-compositor";
    auto const request = enable ? "enableProximityHandling" :
                                  "disableProximityHandling";
    invoke_with_reply(request,
                      DBUS_TYPE_STRING, &name,
                      DBUS_TYPE_INVALID);

    proximity_enabled = enable;
}

bool usc::PowerdMediator::is_valid_brightness(int brightness)
{
    return brightness >= min_brightness_ && brightness <= max_brightness_;
}

bool usc::PowerdMediator::request_suspend_block()
{
    if (suspend_block_cookie.empty())
    {
        char const* const name{"com.canonical.Unity.Screen"};
        int const sys_state_active{static_cast<int>(SysState::active)};

        auto reply = invoke_with_reply(
            "requestSysState",
            DBUS_TYPE_STRING, &name,
            DBUS_TYPE_INT32, &sys_state_active,
            DBUS_TYPE_INVALID);

        if (reply)
        {
            char const* cookie{nullptr};

            dbus_message_get_args(
                reply, nullptr,
                DBUS_TYPE_STRING, &cookie,
                DBUS_TYPE_INVALID);

            if (cookie)
                suspend_block_cookie = cookie;

            return true;
        }
        else
        {
            pending_suspend_block_request = true;
        }
    }

    return false;
}

void usc::PowerdMediator::update_sys_state(SysState state)
{
    {
        std::lock_guard<decltype(mutex)> lock{sys_state_mutex};
        sys_state = state;
    }
    sys_state_changed.notify_one();
}

void usc::PowerdMediator::wait_for_sys_state(SysState state)
{
    std::unique_lock<decltype(sys_state_mutex)> lock{sys_state_mutex};
    sys_state_changed.wait(lock, [this, state]{ return sys_state == state; });
}

void usc::PowerdMediator::update_current_brightness_for_state(BacklightState state)
{
    switch (state)
    {
        case BacklightState::normal:
            current_brightness = normal_brightness_;
            break;
        case BacklightState::off:
            current_brightness = 0;
            break;
        case BacklightState::dim:
            current_brightness = dim_brightness_;
            break;
        default:
            break;
    }
}

usc::DBusMessageHandle usc::PowerdMediator::invoke_with_reply(
    char const* method, int first_arg_type, ...)
{
    va_list args;
    va_start(args, first_arg_type);
    auto msg = make_powerd_method_call_message(method, first_arg_type, args);
    va_end(args);

    std::promise<DBusMessage*> reply_promise;
    auto reply_future = reply_promise.get_future();

    auto const send_message_with_reply =
        [this, &msg, &reply_promise]
        {
            auto const reply = dbus_connection_send_with_reply_and_block(
                connection, msg, DBUS_TIMEOUT_USE_DEFAULT, nullptr);

            reply_promise.set_value(reply);
        };

    // Run in the context of the dbus event loop to avoid
    // strange dbus behaviors.
    if (std::this_thread::get_id() == dbus_loop_thread.get_id())
        send_message_with_reply();
    else
        dbus_event_loop.enqueue(send_message_with_reply);

    return usc::DBusMessageHandle{reply_future.get()};
}
