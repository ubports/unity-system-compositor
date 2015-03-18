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

#ifndef USC_UNITY_SCREEN_SERVICE_H_
#define USC_UNITY_SCREEN_SERVICE_H_

#include "dbus_event_loop.h"
#include "dbus_connection_handle.h"

#include <mir_toolkit/common.h>

#include <cstdint>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

enum class PowerStateChangeReason;

namespace usc
{
class Screen;
class WorkerThread;

class UnityScreenService
{
public:
    UnityScreenService(
        std::string const& bus_addr,
        std::shared_ptr<usc::Screen> const& screen);
    ~UnityScreenService();

private:
    void dbus_loop();
    void stop_loop();
    static ::DBusHandlerResult handle_dbus_message_thunk(
        DBusConnection* connection, DBusMessage* message, void* user_data);
    ::DBusHandlerResult handle_dbus_message(
        DBusConnection* connection, DBusMessage* message, void* user_data);

    void dbus_setUserBrightness(int32_t brightness);
    void dbus_userAutobrightnessEnable(dbus_bool_t enable);
    void dbus_setInactivityTimeouts(int32_t poweroff_timeout, int32_t dimmer_timeout);
    void dbus_setTouchVisualizationEnabled(dbus_bool_t enable);
    bool dbus_setScreenPowerMode(std::string const& mode, int32_t reason);
    int32_t dbus_keepDisplayOn(std::string const& sender);
    void dbus_removeDisplayOnRequest(std::string const& sender, int32_t id);
    void dbus_NameOwnerChanged(
        std::string const& name,
        std::string const& old_owner,
        std::string const& new_owner);
    void dbus_emit_DisplayPowerStateChange(
        MirPowerMode power_mode, PowerStateChangeReason reason);

    DBusConnectionHandle const connection;
    std::shared_ptr<usc::Screen> const screen;

    DBusEventLoop dbus_event_loop;
    std::thread dbus_loop_thread;
    std::mutex keep_display_on_mutex;
    std::unordered_multimap<std::string,int32_t> keep_display_on_ids;
    int32_t request_id;
};

}

#endif
