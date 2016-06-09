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
#include "server.h"
#include "dm_connection.h"

#include <mir/input/composite_event_filter.h>
#include <mir/input/input_device_hub.h>
#include <mir/abnormal_exit.h>

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

}

usc::SystemCompositor::SystemCompositor(
    std::shared_ptr<Server> const& server)
    : server{server},
      spinner{server->the_spinner()}
{
}

void usc::SystemCompositor::run()
{
    if (server->show_version())
    {
        std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
        return;
    }

    server->add_init_callback([&]
        {
            auto vendor = (char *) glGetString(GL_VENDOR);
            auto renderer = (char *) glGetString (GL_RENDERER);
            auto version = (char *) glGetString (GL_VERSION);

            std::cerr << "GL_VENDOR = " << vendor << std::endl;
            std::cerr << "GL_RENDERER = " << renderer << std::endl;
            std::cerr << "GL_VERSION = " << version << std::endl;

            if (!check_blacklist(server->blacklist(), vendor, renderer, version))
            {
                BOOST_THROW_EXCEPTION(
                    mir::AbnormalExit("Video driver is blacklisted, exiting"));
            }

            dm_connection = server->the_dm_connection();

            // Make socket world-writable, since users need to talk to us.  No worries
            // about race condition, since we are adding permissions, not restricting
            // them.
            if (server->public_socket() && chmod(server->get_socket_file().c_str(), 0777) == -1)
                std::cerr << "Unable to chmod socket file " << server->get_socket_file() << ": " << strerror(errno) << std::endl;

            dm_connection->start();
            screen = server->the_screen();
            unity_display_service = server->the_unity_display_service();
            screen_event_handler = server->the_screen_event_handler();

            auto composite_filter = server->the_composite_event_filter();
            composite_filter->append(screen_event_handler);

            unity_input_service = server->the_unity_input_service();
            dbus_service_thread = server->the_dbus_connection_thread();
        });

    server->run();
}
