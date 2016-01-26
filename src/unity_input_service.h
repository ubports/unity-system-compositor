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

#ifndef USC_UNITY_INPUT_SERVICE_H_
#define USC_UNITY_INPUT_SERVICE_H_

#include <dbus/dbus.h>
#include "dbus_connection_handle.h"
#include <memory>

namespace usc
{
class DBusEventLoop;
class InputConfiguration;

class UnityInputService
{
public:
    UnityInputService(
        std::shared_ptr<usc::DBusEventLoop> const& loop,
        std::string const& address,
        std::shared_ptr<usc::InputConfiguration> const& input_config);

private:
    static ::DBusHandlerResult handle_dbus_message_thunk(
        DBusConnection* connection, DBusMessage* message, void* user_data);
    ::DBusHandlerResult handle_dbus_message(
        DBusConnection* connection, DBusMessage* message, void* user_data);

    void handle_message(DBusMessage* message, void (usc::InputConfiguration::* method)(bool));
    void handle_message(DBusMessage* message, void (usc::InputConfiguration::* method)(int32_t));
    void handle_message(DBusMessage* message, void (usc::InputConfiguration::* method)(double));

    std::shared_ptr<usc::DBusEventLoop> const loop;
    std::shared_ptr<usc::DBusConnectionHandle> connection;
    std::shared_ptr<usc::InputConfiguration> const input_config;
};

}

#endif
