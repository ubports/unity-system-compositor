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

#ifndef USC_POWERD_MEDIATOR_H_
#define USC_POWERD_MEDIATOR_H_

#include "screen_hardware.h"

#include "dbus_connection_handle.h"
#include "dbus_event_loop.h"

namespace usc
{
class DBusMessageHandle;

class PowerdMediator : public ScreenHardware
{
public:
    PowerdMediator(std::string const& bus_addr);
    ~PowerdMediator();

    void set_dim_backlight() override;
    void set_normal_backlight() override;
    void turn_off_backlight() override;
    void change_backlight_values(int dim_brightness, int normal_brightness) override;

    void allow_suspend() override;
    void disable_suspend() override;

    void enable_auto_brightness(bool flag) override;
    bool auto_brightness_supported() override;
    void set_brightness(int brightness) override;
    int min_brightness() override;
    int max_brightness() override;

    bool is_system_suspended();

private:
    enum class BacklightState
    {
        off,
        dim,
        normal,
        automatic
    };

    enum class SysState
    {
        unknown = -1,
        suspend = 0,
        active,
    };

    enum class ForceDisableSuspend { no, yes };
    enum class ForceBacklightState { no, yes };

    static ::DBusHandlerResult handle_dbus_message_thunk(
        ::DBusConnection* connection, ::DBusMessage* message, void* user_data);

    ::DBusHandlerResult handle_dbus_message(
        ::DBusConnection* connection, ::DBusMessage* message, void* user_data);
    void init_powerd_state(ForceDisableSuspend force_disable_suspend);
    void init_brightness_params();
    void change_backlight_state(
        BacklightState new_state, ForceBacklightState force_backlight_state);
    bool is_valid_brightness(int brightness);
    bool request_suspend_block();
    void wait_for_sys_state(SysState state);
    void update_sys_state(SysState state);
    void update_current_brightness_for_state(BacklightState state);
    void invoke(char const* method, int first_arg_type, ...);
    usc::DBusMessageHandle invoke_with_reply(char const* method, int first_arg_type, ...);

    usc::DBusConnectionHandle connection;
    usc::DBusEventLoop dbus_event_loop;
    std::thread dbus_loop_thread;

    std::mutex mutex;
    bool pending_suspend_block_request;
    int dim_brightness_;
    int min_brightness_;
    int max_brightness_;
    int normal_brightness_;
    int current_brightness;
    BacklightState backlight_state;
    bool auto_brightness_supported_;
    bool auto_brightness_requested;
    std::string suspend_block_cookie;
    std::mutex sys_state_mutex;
    SysState sys_state;
    std::condition_variable sys_state_changed;
};

}

#endif
