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
#include "asio_dm_connection.h"
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
#include <mir/abnormal_exit.h>
#include <mir/observer_registrar.h>
#include <mir/server.h>

#include <boost/exception/all.hpp>

#include <cerrno>
#include <iostream>
#include <sys/stat.h>
#include <regex.h>
#include <GLES2/gl2.h>

namespace
{

bool check_blacklist(
    std::string const& blacklist,
    const char *vendor,
    const char *renderer,
    const char *version)
{
    if (blacklist.empty())
        return true;

    std::cerr << "Using blacklist \"" << blacklist << "\"" << std::endl;

    regex_t re;
    auto result = regcomp (&re, blacklist.c_str(), REG_EXTENDED);
    if (result == 0)
    {
        char driver_string[1024];
        snprintf (driver_string, 1024, "%s\n%s\n%s",
                  vendor ? vendor : "",
                  renderer ? renderer : "",
                  version ? version : "");

        auto result = regexec (&re, driver_string, 0, nullptr, 0);
        regfree (&re);

        if (result == 0)
            return false;
    }
    else
    {
        char error_string[1024];
        regerror (result, &re, error_string, 1024);
        std::cerr << "Failed to compile blacklist regex: " << error_string << std::endl;
    }

    return true;
}

std::string dbus_bus_address()
{
    static char const* const default_bus_address{"unix:path=/var/run/dbus/system_bus_socket"};

    char const* bus = getenv("DBUS_SYSTEM_BUS_ADDRESS");
    if (!bus)
        bus = default_bus_address;

    return std::string{bus};
}

struct NullDMMessageHandler : usc::DMConnection
{
    explicit NullDMMessageHandler(
        std::shared_ptr<usc::DMMessageHandler> const& dm_message_handler,
        std::string const& client_name) :
        dm_message_handler{dm_message_handler},
        client_name{client_name}
    {}

    ~NullDMMessageHandler() = default;

    void start() override
    {
        dm_message_handler->set_active_session(client_name);
    };

    std::shared_ptr<usc::DMMessageHandler> const dm_message_handler;
    std::string const client_name;
};
}

usc::SystemCompositor::SystemCompositor(
    std::function<std::shared_ptr<SessionSwitcher>()> the_session_switcher)
    : the_session_switcher{the_session_switcher}
{
}

void usc::SystemCompositor::operator()(mir::Server& server)
{
    server.add_init_callback([&]
        {
            auto vendor = (char *) glGetString(GL_VENDOR);
            auto renderer = (char *) glGetString (GL_RENDERER);
            auto version = (char *) glGetString (GL_VERSION);

            std::cerr << "GL_VENDOR = " << vendor << std::endl;
            std::cerr << "GL_RENDERER = " << renderer << std::endl;
            std::cerr << "GL_VERSION = " << version << std::endl;

            auto blacklist = server.get_options()->get("blacklist", "");

            if (!check_blacklist(blacklist, vendor, renderer, version))
            {
                BOOST_THROW_EXCEPTION(
                    mir::AbnormalExit("Video driver is blacklisted, exiting"));
            }

            if (server.get_options()->is_set("from-dm-fd") &&
                server.get_options()->is_set("to-dm-fd"))
            {
                dm_connection = std::make_shared<usc::AsioDMConnection>(
                    server.get_options()->get("from-dm-fd", -1),
                    server.get_options()->get("to-dm-fd", -1),
                    the_session_switcher());
            }
            else if (server.get_options()->is_set("debug-without-dm"))
            {
                dm_connection = std::make_shared<NullDMMessageHandler>(
                    the_session_switcher(),
                    server.get_options()->get<std::string>("debug-active-session-name"));
            }
            else
            {
                BOOST_THROW_EXCEPTION(mir::AbnormalExit("to and from FDs are required for display manager"));
            }

            // Make socket world-writable, since users need to talk to us.  No worries
            // about race condition, since we are adding permissions, not restricting
            // them.
            auto public_socket = !server.get_options()->is_set("no-file") &&
                                 server.get_options()->get("public-socket", true);

            auto socket_file = server.get_options()->get("file", "/tmp/mir_socket");
            if (public_socket && chmod(socket_file.c_str(), 0777) == -1)
                std::cerr << "Unable to chmod socket file " << socket_file << ": " << strerror(errno) << std::endl;

            dm_connection->start();

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
