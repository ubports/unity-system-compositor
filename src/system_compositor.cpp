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

#include "dbus_screen.h"
#include "system_compositor.h"

#include <mir/run_mir.h>
#include <mir/abnormal_exit.h>
#include <mir/default_server_configuration.h>
#include <mir/frontend/shell.h>
#include <mir/server_status_listener.h>
#include <mir/shell/session.h>
#include <mir/shell/focus_controller.h>
#include <mir/input/cursor_listener.h>

#include <cerrno>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <regex.h>
#include <GLES2/gl2.h>
#include <boost/algorithm/string.hpp>
#include <QCoreApplication>

namespace msh = mir::shell;
namespace mf = mir::frontend;
namespace mi = mir::input;
namespace po = boost::program_options;

class SystemCompositorShell : public mf::Shell
{
public:
    SystemCompositorShell(std::shared_ptr<mf::Shell> const& self,
                          std::shared_ptr<msh::FocusController> const& focus_controller)
        : self(self), focus_controller{focus_controller} {}

    std::shared_ptr<mf::Session> session_named(std::string const& name)
    {
        return sessions[name];
    }

    void set_active_session(std::string const& name)
    {
        active_session = name;

        if (auto session = std::static_pointer_cast<msh::Session>(session_named(name)))
            focus_controller->set_focus_to(session);
        else
            std::cerr << "Unable to set active session, unknown client name " << name << std::endl;
    }

    void set_next_session(std::string const& name)
    {
        if (auto const session = std::static_pointer_cast<msh::Session>(session_named(name)))
        {
            focus_controller->set_focus_to(session); // raise session inside its depth id set
            set_active_session(active_session); // to restore input focus to where it should be
        }
        else
            std::cerr << "Unable to set next session, unknown client name " << name << std::endl;
    }

private:
    std::shared_ptr<mf::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<mf::EventSink> const& sink)
    {
        auto result = self->open_session(client_pid, name, sink);
        sessions[name] = result;

        // Opening a new session will steal focus from our active session, so
        // restore the focus if needed.
        set_active_session(active_session);

        return result;
    }

    void close_session(std::shared_ptr<mf::Session> const& session)
    {
        sessions.erase(session->name());
        self->close_session(session);
    }

    mf::SurfaceId create_surface_for(
        std::shared_ptr<mf::Session> const& session,
        msh::SurfaceCreationParameters const& params)
    {
        return self->create_surface_for(session, params);
    }

    void handle_surface_created(std::shared_ptr<mf::Session> const& session)
    {
        self->handle_surface_created(session);
    }

    std::shared_ptr<mf::Shell> const self;
    std::shared_ptr<msh::FocusController> const focus_controller;
    std::map<std::string, std::shared_ptr<mf::Session>> sessions;
    std::string active_session;
};

class SystemCompositorServerConfiguration : public mir::DefaultServerConfiguration
{
public:
    SystemCompositorServerConfiguration(SystemCompositor *compositor, int argc, char **argv)
        : mir::DefaultServerConfiguration(argc, (char const **)argv), compositor{compositor}
    {
        add_options()
            ("from-dm-fd", po::value<int>(),  "File descriptor of read end of pipe from display manager [int]")
            ("to-dm-fd", po::value<int>(),  "File descriptor of write end of pipe to display manager [int]")
            ("blacklist", po::value<std::string>(), "Video blacklist regex to use")
            ("version", "Show version of Unity System Compositor")
            ("public-socket", po::value<bool>(), "Make the socket file publicly writable");
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

    bool public_socket()
    {
        return !the_options()->is_set("no-file") && the_options()->get("public-socket", true);
    }

    void parse_options(boost::program_options::options_description& options_description, mir::options::ProgramOption& options) const override
    {
        setenv("MIR_SERVER_STANDALONE", "true", 0); // Default to standalone
        mir::DefaultServerConfiguration::parse_options(options_description, options);
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

    std::shared_ptr<mir::ServerStatusListener> the_server_status_listener() override
    {
        struct ServerStatusListener : public mir::ServerStatusListener
        {
            ServerStatusListener (SystemCompositor *compositor) : compositor{compositor} {}

            void paused() override
            {
                compositor->pause();
            }

            void resumed() override
            {
                compositor->resume();
            }

            void started() override
            {
            }

            SystemCompositor *compositor;
        };
        return std::make_shared<ServerStatusListener>(compositor);
    }

    std::string get_socket_file()
    {
        // the_socket_file is private, so we have to re-implement it here
        return the_options()->get("file", "/tmp/mir_socket");
    }

    std::shared_ptr<SystemCompositorShell> the_system_compositor_shell()
    {
        return sc_shell([this]
        {
            return std::make_shared<SystemCompositorShell>(
                mir::DefaultServerConfiguration::the_frontend_shell(),
                the_focus_controller());
        });
    }

private:
    mir::CachedPtr<SystemCompositorShell> sc_shell;

    std::shared_ptr<mf::Shell> the_frontend_shell() override
    {
        return the_system_compositor_shell();
    }

    SystemCompositor *compositor;
};

bool check_blacklist(std::string blacklist, const char *vendor, const char *renderer, const char *version)
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

void SystemCompositor::run(int argc, char **argv)
{
    config = std::make_shared<SystemCompositorServerConfiguration>(this, argc, argv);
  
    if (config->show_version())
    {
        std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
        return;
    }

    dm_connection = std::make_shared<DMConnection>(io_service, config->from_dm_fd(), config->to_dm_fd());

    struct ScopeGuard
    {
        explicit ScopeGuard(boost::asio::io_service& io_service) : io_service(io_service) {}
        ~ScopeGuard()
        {
            io_service.stop();
            if (io_thread.joinable())
                io_thread.join();
            if (qt_thread.joinable())
                qt_thread.join();
        }

        boost::asio::io_service& io_service;
        std::thread io_thread;
        std::thread qt_thread;
    } guard(io_service);

    mir::run_mir(*config, [&](mir::DisplayServer&)
        {
            auto vendor = (char *) glGetString(GL_VENDOR);
            auto renderer = (char *) glGetString (GL_RENDERER);
            auto version = (char *) glGetString (GL_VERSION);
            std::cerr << "GL_VENDOR = " << vendor << std::endl;
            std::cerr << "GL_RENDERER = " << renderer << std::endl;
            std::cerr << "GL_VERSION = " << version << std::endl;

            if (!check_blacklist(config->blacklist(), vendor, renderer, version))
                throw mir::AbnormalExit ("Video driver is blacklisted, exiting");

            shell = config->the_system_compositor_shell();
            guard.io_thread = std::thread(&SystemCompositor::main, this);
            guard.qt_thread = std::thread(&SystemCompositor::qt_main, this, argc, argv);
        });
}

void SystemCompositor::pause()
{
    std::cerr << "pause" << std::endl;

    if (auto active_session = config->the_focus_controller()->focussed_application().lock())
        active_session->set_lifecycle_state(mir_lifecycle_state_will_suspend);
}

void SystemCompositor::resume()
{
    std::cerr << "resume" << std::endl;

    if (auto active_session = config->the_focus_controller()->focussed_application().lock())
        active_session->set_lifecycle_state(mir_lifecycle_state_resumed);
}

void SystemCompositor::set_active_session(std::string client_name)
{
    std::cerr << "set_active_session" << std::endl;
    shell->set_active_session(client_name);
}

void SystemCompositor::set_next_session(std::string client_name)
{
    std::cerr << "set_next_session" << std::endl;
    shell->set_next_session(client_name);
}

void SystemCompositor::main()
{
    // Make socket world-writable, since users need to talk to us.  No worries
    // about race condition, since we are adding permissions, not restricting
    // them.
    if (config->public_socket() && chmod(config->get_socket_file().c_str(), 0777) == -1)
        std::cerr << "Unable to chmod socket file " << config->get_socket_file() << ": " << strerror(errno) << std::endl;

    dm_connection->set_handler(this);
    dm_connection->start();
    dm_connection->send_ready();

    io_service.run();
}

void SystemCompositor::qt_main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    DBusScreen dbus_screen(config);
    app.exec();
}
