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

#ifndef USC_UNITY_DISPLAY_SERVICE_H_
#define USC_UNITY_DISPLAY_SERVICE_H_

#include "dbus_connection_handle.h"
#include "screen.h"

#include <memory>
#include <string>

namespace usc
{
class Screen;
class DBusEventLoop;

class UnityDisplayService
{
public:
    UnityDisplayService(
        std::shared_ptr<usc::DBusEventLoop> const& loop,
        std::string const& address,
        std::shared_ptr<usc::Screen> const& screen);
    ~UnityDisplayService();

private:
    static ::DBusHandlerResult handle_dbus_message_thunk(
        DBusConnection* connection, DBusMessage* message, void* user_data);
    ::DBusHandlerResult handle_dbus_message(
        DBusConnection* connection, DBusMessage* message, void* user_data);

    void dbus_TurnOn();
    void dbus_TurnOff();
    void dbus_emit_ActiveOutputs();
    void dbus_properties_Get(DBusMessage* reply, std::string const& property);
    void dbus_properties_GetAll(DBusMessage* reply);

    std::shared_ptr<usc::Screen> const screen;
    std::shared_ptr<DBusEventLoop> const loop;
    std::shared_ptr<DBusConnectionHandle> connection;
    ActiveOutputs active_outputs;
};

}

#endif
