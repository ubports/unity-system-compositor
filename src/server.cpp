/*
 * Copyright © 2014-2015 Canonical Ltd.
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

#include "server.h"
#include "external_spinner.h"
#include "shell.h"
#include "asio_dm_connection.h"
#include "session_switcher.h"
#include "powerd_mediator.h"

#include <mir/input/cursor_listener.h>
#include <mir/server_status_listener.h>
#include <mir/shell/focus_controller.h>
#include <mir/scene/session.h>

#include <iostream>

namespace msh = mir::shell;
namespace ms = mir::scene;
namespace mf = mir::frontend;
namespace mi = mir::input;

namespace
{
// FIXME For reasons that are not currently clear lightdm sometimes passes "--vt" on android
void ignore_unknown_arguments(int argc, char const* const* argv)
{
    std::cout << "Warning: ignoring unrecognised arguments:";
    for (auto arg = argv; arg != argv+argc; ++arg)
        std::cout << " " << *arg;
    std::cout << std::endl;
}

struct NullCursorListener : public mi::CursorListener
{
    void cursor_moved_to(float, float) override
    {
    }
};

struct ServerStatusListener : public mir::ServerStatusListener
{
    explicit ServerStatusListener(
        std::shared_ptr<msh::FocusController> const& focus_controller)
        : focus_controller{focus_controller}
    {
    }

    void paused() override
    {
        std::cerr << "pause" << std::endl;

        if (auto active_session = weak_active_session().lock())
            active_session->set_lifecycle_state(mir_lifecycle_state_will_suspend);
    }

    void resumed() override
    {
        std::cerr << "resume" << std::endl;

        if (auto active_session = weak_active_session().lock())
            active_session->set_lifecycle_state(mir_lifecycle_state_resumed);
    }

    void started() override
    {
    }

    std::weak_ptr<ms::Session> weak_active_session()
    {
        return focus_controller->focused_session();
    }

    std::shared_ptr<msh::FocusController> const focus_controller;
};
}

usc::Server::Server(int argc, char** argv)
{
    add_configuration_option("from-dm-fd", "File descriptor of read end of pipe from display manager [int]", mir::OptionType::integer);
    add_configuration_option("to-dm-fd",   "File descriptor of write end of pipe to display manager [int]",  mir::OptionType::integer);
    add_configuration_option("blacklist", "Video blacklist regex to use",  mir::OptionType::string);
    add_configuration_option("version", "Show version of Unity System Compositor",  mir::OptionType::null);
    add_configuration_option("spinner", "Path to spinner executable",  mir::OptionType::string);
    add_configuration_option("public-socket", "Make the socket file publicly writable",  mir::OptionType::boolean);
    add_configuration_option("enable-hardware-cursor", "Enable the hardware cursor (disabled by default)",  mir::OptionType::boolean);
    add_configuration_option("inactivity-display-off-timeout", "The time in seconds before the screen is turned off when there are no active sessions",  mir::OptionType::integer);
    add_configuration_option("inactivity-display-dim-timeout", "The time in seconds before the screen is dimmed when there are no active sessions",  mir::OptionType::integer);
    add_configuration_option("shutdown-timeout", "The time in milli-seconds the power key must be held to initiate a clean system shutdown",  mir::OptionType::integer);
    add_configuration_option("power-key-ignore-timeout", "The time in milli-seconds the power key must be held to ignore - must be less than shutdown-timeout",  mir::OptionType::integer);
    add_configuration_option("disable-inactivity-policy", "Disables handling user inactivity and power key",  mir::OptionType::boolean);

    set_command_line(argc, const_cast<char const **>(argv));

    set_command_line_handler(&ignore_unknown_arguments);

    wrap_cursor_listener([this](std::shared_ptr<mir::input::CursorListener> const& default_)
        -> std::shared_ptr<mir::input::CursorListener>
        {
            // This is a workaround for u8 desktop preview in 14.04 for the lack of client cursor API.
            // We need to disable the cursor for XMir but leave it on for the desktop preview.
            // Luckily as it stands they run inside seperate instances of USC. ~racarr
            if (enable_hardware_cursor())
                return default_;
            else
                return std::make_shared<NullCursorListener>();
        });

    override_the_server_status_listener([this]()
        -> std::shared_ptr<mir::ServerStatusListener>
        {
            return std::make_shared<ServerStatusListener>(the_focus_controller());
        });

    wrap_shell([this](std::shared_ptr<msh::Shell> const& wrapped)
        -> std::shared_ptr<msh::Shell>
        {
            return std::make_shared<Shell>(wrapped, the_session_switcher());
        });

    set_config_filename("unity-system-compositor.conf");

    apply_settings();
}

std::shared_ptr<usc::Spinner> usc::Server::the_spinner()
{
    return spinner(
        [this]
        {
            return std::make_shared<ExternalSpinner>(
                spinner_executable(),
                get_socket_file());
        });
}

std::shared_ptr<usc::SessionSwitcher> usc::Server::the_session_switcher()
{
    return session_switcher(
        [this]
        {
            return std::make_shared<SessionSwitcher>(
                the_spinner());
        });
}

std::shared_ptr<usc::DMMessageHandler> usc::Server::the_dm_message_handler()
{
    return the_session_switcher();
}

std::shared_ptr<usc::DMConnection> usc::Server::the_dm_connection()
{
    return dm_connection(
        [this]
        {
            return std::make_shared<AsioDMConnection>(
                the_options()->get("from-dm-fd", -1),
                the_options()->get("to-dm-fd", -1),
                the_dm_message_handler());
        });
}

std::shared_ptr<usc::ScreenHardware> usc::Server::the_screen_hardware()
{
    return screen_hardware(
        [this]
        {
            return std::make_shared<PowerdMediator>();
        });
}
