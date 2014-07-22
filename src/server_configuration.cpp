/*
 * Copyright © 2014 Canonical Ltd.
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

#include "server_configuration.h"
#include "external_spinner.h"
#include "session_coordinator.h"
#include "surface_coordinator.h"
#include "asio_dm_connection.h"
#include "session_switcher.h"

#include <mir/options/default_configuration.h>
#include <mir/input/cursor_listener.h>
#include <mir/server_status_listener.h>
#include <mir/shell/focus_controller.h>
#include <mir/scene/session.h>

#include <boost/program_options.hpp>

namespace msh = mir::shell;
namespace ms = mir::scene;
namespace mf = mir::frontend;
namespace mi = mir::input;
namespace mo = mir::options;
namespace po = boost::program_options;

namespace
{

class ConfigurationOptions : public mo::DefaultConfiguration
{
public:
    ConfigurationOptions(int argc, char const* argv[]) :
        DefaultConfiguration(argc, argv)
    {
        add_options()
            ("from-dm-fd", po::value<int>(),  "File descriptor of read end of pipe from display manager [int]")
            ("to-dm-fd", po::value<int>(),  "File descriptor of write end of pipe to display manager [int]")
            ("blacklist", po::value<std::string>(), "Video blacklist regex to use")
            ("version", "Show version of Unity System Compositor")
            ("spinner", po::value<std::string>(), "Path to spinner executable")
            ("public-socket", po::value<bool>(), "Make the socket file publicly writable")
            ("enable-hardware-cursor", po::value<bool>(), "Enable the hardware cursor (disabled by default)")
            ("inactivity-display-off-timeout", po::value<int>(), "The time in seconds before the screen is turned off when there are no active sessions")
            ("inactivity-display-dim-timeout", po::value<int>(), "The time in seconds before the screen is dimmed when there are no active sessions")
            ("shutdown-timeout", po::value<int>(), "The time in milli-seconds the power key must be held to initiate a clean system shutdown")
            ("power-key-ignore-timeout", po::value<int>(), "The time in milli-seconds the power key must be held to ignore - must be less than shutdown-timeout")
            ("disable-inactivity-policy", po::value<bool>(), "Disables handling user inactivity and power key");
    }

    void parse_config_file(
        boost::program_options::options_description& options_description,
        mo::ProgramOption& options) const override
    {
        options.parse_file(options_description, "unity-system-compositor.conf");
    }
};

}

usc::ServerConfiguration::ServerConfiguration(int argc, char** argv)
    : mir::DefaultServerConfiguration(
        std::make_shared<ConfigurationOptions>(argc, const_cast<char const **>(argv)))
{
}

std::shared_ptr<mi::CursorListener>
usc::ServerConfiguration::the_cursor_listener()
{
    struct NullCursorListener : public mi::CursorListener
    {
        void cursor_moved_to(float, float) override
        {
        }
    };

    // This is a workaround for u8 desktop preview in 14.04 for the lack of client cursor API.
    // We need to disable the cursor for XMir but leave it on for the desktop preview.
    // Luckily as it stands they run inside seperate instances of USC. ~racarr
    if (enable_hardware_cursor())
        return mir::DefaultServerConfiguration::the_cursor_listener();
    else
        return std::make_shared<NullCursorListener>();
}

std::shared_ptr<mir::ServerStatusListener>
usc::ServerConfiguration::the_server_status_listener()
{
    struct ServerStatusListener : public mir::ServerStatusListener
    {
        ServerStatusListener(
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
            return focus_controller->focussed_application();
        }

        std::shared_ptr<msh::FocusController> const focus_controller;
    };

    return std::make_shared<ServerStatusListener>(the_focus_controller());
}

std::shared_ptr<mir::scene::SessionCoordinator>
usc::ServerConfiguration::wrap_session_coordinator(
    std::shared_ptr<ms::SessionCoordinator> const& wrapped)
{
    return std::make_shared<SessionCoordinator>(
        wrapped,
        the_session_switcher());
}

std::shared_ptr<mir::scene::SurfaceCoordinator>
usc::ServerConfiguration::wrap_surface_coordinator(
    std::shared_ptr<ms::SurfaceCoordinator> const& wrapped)
{
    return std::make_shared<SurfaceCoordinator>(
        wrapped,
        the_session_switcher());
}

std::shared_ptr<usc::Spinner> usc::ServerConfiguration::the_spinner()
{
    return spinner(
        [this]
        {
            return std::make_shared<ExternalSpinner>(
                spinner_executable(),
                get_socket_file());
        });
}

std::shared_ptr<usc::SessionSwitcher> usc::ServerConfiguration::the_session_switcher()
{
    return session_switcher(
        [this]
        {
            return std::make_shared<SessionSwitcher>(
                the_spinner());
        });
}

std::shared_ptr<usc::DMMessageHandler> usc::ServerConfiguration::the_dm_message_handler()
{
    return the_session_switcher();
}

std::shared_ptr<usc::DMConnection> usc::ServerConfiguration::the_dm_connection()
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
