/*
 * Copyright © 2013 Canonical Ltd.
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
 */

#include "system_compositor.h"

#include <mir/run_mir.h>
#include <mir/abnormal_exit.h>
#include <mir/shell/session.h>
#include <mir/shell/session_container.h>
#include <mir/shell/focus_setter.h>
#include <mir/input/cursor_listener.h>

#include <iostream>
#include <thread>
#include <regex.h>
#include <GLES2/gl2.h>
#include <boost/algorithm/string.hpp>

namespace msh = mir::shell;
namespace mi = mir::input;
namespace po = boost::program_options;

class SystemCompositorServerConfiguration : public mir::DefaultServerConfiguration
{
public:
    SystemCompositorServerConfiguration(int argc, char const** argv)
        : mir::DefaultServerConfiguration(argc, argv)
    {
        add_options()
            ("from-dm-fd", po::value<int>(),  "File descriptor of read end of pipe from display manager [int]")
            ("to-dm-fd", po::value<int>(),  "File descriptor of write end of pipe to display manager [int]")
            ("blacklist", po::value<std::string>(), "Video blacklist regex to use")
            ("version", "Show version of Unity System Compositor");
    }

    int from_dm_fd()
    {
        return the_options()->get("from-dm-fd", -1);
    }

    int to_dm_fd()
    {
        return the_options()->get("to-dm-fd", -1);
    }

    bool show_version()
    {
        return the_options()->is_set ("version");
    }

    std::string blacklist()
    {
        auto x = the_options()->get ("blacklist", "");
        boost::trim(x);
        return x;
    }

    void parse_options(boost::program_options::options_description& options_description, mir::options::ProgramOption& options) const override
    {
        options.parse_file(options_description, "unity-system-compositor.conf");
    }

    std::shared_ptr<mi::CursorListener> the_cursor_listener() override
    {
        struct NullCursorListener : public mi::CursorListener
        {
            void cursor_moved_to(float, float) override
            {
            }
        };
        return std::make_shared<NullCursorListener>();
    }
};

bool check_blacklist(std::string blacklist, const char *vendor, const char *renderer, const char *version)
{
    if (blacklist.empty())
        return true;

    regex_t re;
    auto result = regcomp (&re, blacklist.c_str(), REG_EXTENDED);
    if (result == 0)
    {
        char driver_string[1024];
        snprintf (driver_string, 1024, "%s\n%s\n%s",
                  vendor ? vendor : "",
                  renderer ? renderer : "",
                  version ? version : "");

        auto result = regexec (&re, driver_string, 0, NULL, 0);
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

void SystemCompositor::run(int argc, char const** argv)
{
    auto c = std::make_shared<SystemCompositorServerConfiguration>(argc, argv);
    config = c;
  
    if (c->show_version())
    {
        std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
        return;
    }

    dm_connection = std::make_shared<DMConnection>(io_service, c->from_dm_fd(), c->to_dm_fd());

    struct ScopeGuard
    {
        explicit ScopeGuard(boost::asio::io_service& io_service) : io_service(io_service) {}
        ~ScopeGuard() { io_service.stop(); if (thread.joinable()) thread.join(); }

        boost::asio::io_service& io_service;
        std::thread thread;
    } guard(io_service);

    mir::run_mir(*config, [&](mir::DisplayServer&)
        {
            auto vendor = (char *) glGetString(GL_VENDOR);
            auto renderer = (char *) glGetString (GL_RENDERER);
            auto version = (char *) glGetString (GL_VERSION);
            std::cerr << "GL_VENDOR = " << vendor << std::endl;
            std::cerr << "GL_RENDERER = " << renderer << std::endl;
            std::cerr << "GL_VERSION = " << version << std::endl;

            if (!check_blacklist(c->blacklist(), vendor, renderer, version))
                throw mir::AbnormalExit ("Video driver is blacklisted, exiting");

            guard.thread = std::thread(&SystemCompositor::main, this);
        });
}

void SystemCompositor::set_active_session(std::string client_name)
{
    std::cerr << "set_active_session" << std::endl;

    std::shared_ptr<msh::Session> session;
    config->the_shell_session_container()->for_each([&client_name, &session](std::shared_ptr<msh::Session> const& s)
    {
        if (s->name() == client_name)
            session = s;
    });

    if (session)
        config->the_shell_focus_setter()->set_focus_to(session);
    else
        std::cerr << "Unable to set active session, unknown client name " << client_name << std::endl;
}

void SystemCompositor::set_next_session(std::string client_name)
{
    std::cerr << "set_next_session" << std::endl;

    std::shared_ptr<msh::Session> session;
    config->the_shell_session_container()->for_each([&client_name, &session](std::shared_ptr<msh::Session> const& s)
    {
        if (s->name() == client_name)
            session = s;
    });

    if (session)
        ; // TODO: implement this
    else
        std::cerr << "Unable to set next session, unknown client name " << client_name << std::endl;
}

void SystemCompositor::main()
{
    dm_connection->set_handler(this);
    dm_connection->start();
    dm_connection->send_ready();

    io_service.run();
}
