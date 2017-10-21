/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Robert Ancell <robert.ancell@canonical.com>
 *              Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "system_compositor.h"
#include "screen_event_handler.h"
#include "session_switcher.h"
#include "steady_clock.h"
#include "dbus_connection_thread.h"
#include "dbus_event_loop.h"
#include "mir_input_configuration.h"
#include "mir_screen.h"
#include "unity_display_service.h"
#include "unity_input_service.h"
#include "unity_power_button_event_sink.h"
#include "unity_user_activity_event_sink.h"

#include <mir/input/composite_event_filter.h>
#include <mir/input/input_device_hub.h>
#include <mir/options/option.h>
#include <mir/observer_registrar.h>
#include <mir/server.h>

#include <boost/exception/all.hpp>

#include <cerrno>
#include <iostream>
#include <regex.h>
#include <GLES2/gl2.h>

namespace
{

std::string dbus_bus_address()
{
    static char const* const default_bus_address{"unix:path=/var/run/dbus/system_bus_socket"};

    char const* bus = getenv("DBUS_SYSTEM_BUS_ADDRESS");
    if (!bus)
        bus = default_bus_address;

    return std::string{bus};
}
}

void usc::SystemCompositor::operator()(mir::Server& server)
{
    server.add_init_callback([&]
        {
            auto the_screen = std::make_shared<MirScreen>(
                                  server.the_compositor(),
                                  server.the_display());

            server.the_display_configuration_observer_registrar()->register_interest(the_screen);

            screen = the_screen;

            auto the_dbus_event_loop = std::make_shared<DBusEventLoop>();

            unity_display_service = std::make_shared<UnityDisplayService>(
                                        the_dbus_event_loop,
                                        dbus_bus_address(),
                                        screen);

            auto the_power_button_event_sink =
                std::make_shared<usc::UnityPowerButtonEventSink>(
                    dbus_bus_address());

            auto the_user_activity_event_sink =
                std::make_shared<usc::UnityUserActivityEventSink>(
                    dbus_bus_address());

            auto the_clock = std::make_shared<SteadyClock>();

            screen_event_handler = std::make_shared<usc::ScreenEventHandler>(
                                       the_power_button_event_sink,
                                       the_user_activity_event_sink,
                                       the_clock);

            auto composite_filter = server.the_composite_event_filter();
            composite_filter->append(screen_event_handler);

            auto the_input_configuration =
                std::make_shared<MirInputConfiguration>(
                    server.the_input_device_hub());

            unity_input_service = std::make_shared<UnityInputService>(
                                      the_dbus_event_loop,
                                      dbus_bus_address(),
                                      the_input_configuration);

            dbus_service_thread = std::make_shared<DBusConnectionThread>(
                                      the_dbus_event_loop);
        });
}
