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

#define MIR_LOG_COMPONENT "UnitySystemCompositor"

#include "server.h"
#include "external_spinner.h"
#include "asio_dm_connection.h"
#include "session_switcher.h"
#include "window_manager.h"
#include "mir_screen.h"
#include "mir_input_configuration.h"
#include "screen_event_handler.h"
#include "unity_display_service.h"
#include "unity_input_service.h"
#include "unity_power_button_event_sink.h"
#include "unity_user_activity_event_sink.h"
#include "dbus_connection_thread.h"
#include "dbus_event_loop.h"
#include "display_configuration_policy.h"
#include "steady_clock.h"

#include <mir/cookie/authority.h>
#include <mir/input/cursor_listener.h>
#include <mir/server_status_listener.h>
#include <mir/shell/focus_controller.h>
#include <mir/scene/session.h>
#include <mir/log.h>
#include <mir/abnormal_exit.h>
#include <mir/main_loop.h>
#include <mir/observer_registrar.h>

#include <boost/exception/all.hpp>

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

    void ready_for_user_input() override
    {
    }

    void stop_receiving_input() override
    {
    }

    std::weak_ptr<ms::Session> weak_active_session()
    {
        return focus_controller->focused_session();
    }

    std::shared_ptr<msh::FocusController> const focus_controller;
};

struct StubCookie : public mir::cookie::Cookie
{
    uint64_t timestamp() const override
    {
        return 0;
    }

    std::vector<uint8_t> serialize() const override
    {
        // FIXME Workaround until mir fixes its internal message bliting
        // This size needs to equal mir/src/include/cookie/mir/cookie/blob.h::default_blob_size
        return std::vector<uint8_t>(29);
    }
};

struct StubCookieAuthority : public mir::cookie::Authority
{
    std::unique_ptr<mir::cookie::Cookie> make_cookie(uint64_t const& timestamp) override
    {
        return std::unique_ptr<StubCookie>(new StubCookie());
    }

    std::unique_ptr<mir::cookie::Cookie> make_cookie(std::vector<uint8_t> const& raw_cookie) override
    {
        return std::unique_ptr<StubCookie>(new StubCookie());
    }
};

const char* const dm_from_fd = "from-dm-fd";
const char* const dm_to_fd = "to-dm-fd";
const char* const dm_stub = "debug-without-dm";
const char* const dm_stub_active = "debug-active-session-name";
}

usc::Server::Server(int argc, char** argv)
{
    add_configuration_option(dm_from_fd, "File descriptor of read end of pipe from display manager [int]",
        mir::OptionType::integer);
    add_configuration_option(dm_to_fd, "File descriptor of write end of pipe to display manager [int]",
        mir::OptionType::integer);
    add_configuration_option(dm_stub, "Run without a display manager (only useful when debugging)", mir::OptionType::null);
    add_configuration_option(dm_stub_active, "Expected connection when run without a display manager (only useful when debugging)", "nested-mir@:/run/user/1000/mir_socket");
    add_configuration_option("blacklist", "Video blacklist regex to use",  mir::OptionType::string);
    add_configuration_option("version", "Show version of Unity System Compositor",  mir::OptionType::null);
    add_configuration_option("spinner", "Path to spinner executable",  mir::OptionType::string);
    add_configuration_option("public-socket", "Make the socket file publicly writable",  mir::OptionType::boolean);
    add_configuration_option("enable-hardware-cursor", "Enable the hardware cursor (disabled by default)",  mir::OptionType::boolean);
    add_display_configuration_options_to(*this);

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

    override_the_window_manager_builder([this](msh::FocusController* focus_controller)
       {
         return std::make_shared<WindowManager>(
             focus_controller,
             the_shell_display_layout(),
             the_session_coordinator(),
             the_session_switcher());
       });

    override_the_cookie_authority([this]()
    -> std::shared_ptr<mir::cookie::Authority>
    {
        return std::make_unique<StubCookieAuthority>();
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

namespace
{
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

std::shared_ptr<usc::DMConnection> usc::Server::the_dm_connection()
{
    return dm_connection(
        [this]() -> std::shared_ptr<usc::DMConnection>
        {
            if (the_options()->is_set(dm_from_fd) && the_options()->is_set(dm_to_fd))
            {
                return std::make_shared<AsioDMConnection>(
                    the_options()->get(dm_from_fd, -1),
                    the_options()->get(dm_to_fd, -1),
                    the_dm_message_handler());
            }
            else if (the_options()->is_set(dm_stub))
            {
                return std::make_shared<NullDMMessageHandler>(
                    the_dm_message_handler(),
                    the_options()->get<std::string>(dm_stub_active));
            }

            BOOST_THROW_EXCEPTION(mir::AbnormalExit("to and from FDs are required for display manager"));
        });
}

std::shared_ptr<usc::InputConfiguration> usc::Server::the_input_configuration()
{
    return input_configuration(
        [this]
        {
            return std::make_shared<MirInputConfiguration>(the_input_device_hub());
        });
}

std::shared_ptr<usc::Screen> usc::Server::the_screen()
{
    return screen(
        [this]
        {
            auto mir_screen = std::make_shared<MirScreen>(
                the_compositor(),
                the_display());

            the_display_configuration_observer_registrar()->register_interest(mir_screen);

            return mir_screen;
        });
}

std::shared_ptr<mi::EventFilter> usc::Server::the_screen_event_handler()
{
    return screen_event_handler(
        [this]
        {
            return std::make_shared<ScreenEventHandler>(
                the_power_button_event_sink(),
                the_user_activity_event_sink(),
                the_clock());
        });
}

std::shared_ptr<usc::Clock> usc::Server::the_clock()
{
    return clock(
        [this]
        {
            return std::make_shared<SteadyClock>();
        });
}

std::shared_ptr<usc::DBusEventLoop> usc::Server::the_dbus_event_loop()
{
    return dbus_loop(
        [this]
        {
            return std::make_shared<DBusEventLoop>();
        });

}

std::shared_ptr<usc::DBusConnectionThread> usc::Server::the_dbus_connection_thread()
{
    return dbus_thread(
        [this]
        {
            return std::make_shared<DBusConnectionThread>(the_dbus_event_loop());
        });
}

std::shared_ptr<usc::UnityDisplayService> usc::Server::the_unity_display_service()
{
    return unity_display_service(
        [this]
        {
            return std::make_shared<UnityDisplayService>(
                    the_dbus_event_loop(),
                    dbus_bus_address(),
                    the_screen());
        });
}

std::shared_ptr<usc::PowerButtonEventSink> usc::Server::the_power_button_event_sink()
{
    return power_button_event_sink(
        [this]
        {
            return std::make_shared<UnityPowerButtonEventSink>(dbus_bus_address());
        });
}

std::shared_ptr<usc::UserActivityEventSink> usc::Server::the_user_activity_event_sink()
{
    return user_activity_event_sink(
        [this]
        {
            return std::make_shared<UnityUserActivityEventSink>(dbus_bus_address());
        });
}

std::shared_ptr<usc::UnityInputService> usc::Server::the_unity_input_service()
{
    return unity_input_service(
        [this]
        {
            return std::make_shared<UnityInputService>(
                    the_dbus_event_loop(),
                    dbus_bus_address(),
                    the_input_configuration());
        });
}

std::string usc::Server::dbus_bus_address()
{
    static char const* const default_bus_address{"unix:path=/var/run/dbus/system_bus_socket"};

    char const* bus = getenv("DBUS_SYSTEM_BUS_ADDRESS");
    if (!bus)
        bus = default_bus_address;

    return std::string{bus};
}
