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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef USC_TEST_UNITY_DISPLAY_DBUS_CLIENT_H_
#define USC_TEST_UNITY_DISPLAY_DBUS_CLIENT_H_

#include "dbus_client.h"

namespace usc
{
namespace test
{

class UnityDisplayDBusClient : public DBusClient
{
public:
    UnityDisplayDBusClient(std::string const& address);

    DBusAsyncReplyString request_introspection();
    DBusAsyncReplyVoid request_turn_on();
    DBusAsyncReplyVoid request_turn_off();
    DBusAsyncReply request_active_outputs_property();
    DBusAsyncReply request_all_properties();
    DBusAsyncReply request_invalid_method();

    DBusMessageHandle listen_for_properties_changed();

    char const* const unity_display_interface = "com.canonical.Unity.Display";
};

}
}

#endif
