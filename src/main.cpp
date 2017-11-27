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

#include "create_dm_connection.h"
#include "external_spinner.h"
#include "extract_socket_config.h"
#include "session_switcher.h"
#include "system_compositor.h"
#include "validate_video_driver.h"
#include "window_manager.h"

#include <miral/command_line_option.h>
#include <miral/display_configuration_option.h>

#include <mir/cookie/authority.h>
#include <mir/input/cursor_listener.h>
#include <mir/options/option.h>
#include <mir/report_exception.h>
#include <mir/scene/session.h>
#include <mir/shell/focus_controller.h>
#include <mir/server.h>
#include <mir/server_status_listener.h>
#include <iostream>
#include <sys/stat.h>

namespace usc
{
    class DMConnection;
    class SessionSwitcher;
}

// FIXME For reasons that are not currently clear lightdm sometimes passes "--vt" on android
void ignore_unknown_arguments(int argc, char const* const* argv)
{
    std::cout << "Warning: ignoring unrecognised arguments:";
    for (auto arg = argv; arg != argv+argc; ++arg)
        std::cout << " " << *arg;
    std::cout << std::endl;
}

struct NullCursorListener : public mir::input::CursorListener
{
    void cursor_moved_to(float, float) override
    {
    }
};

struct ServerStatusListener : public mir::ServerStatusListener
{
    explicit ServerStatusListener(
        std::shared_ptr<mir::shell::FocusController> const& focus_controller)
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

    std::weak_ptr<mir::scene::Session> weak_active_session()
    {
        return focus_controller->focused_session();
    }

    std::shared_ptr<mir::shell::FocusController> const focus_controller;
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

int main(int argc, char *argv[])
try
{
    auto const config = std::make_shared<mir::Server>();

    auto version_flag = miral::pre_init(miral::CommandLineOption(
        [](bool is_set)
        {
            if (is_set)
            {
                std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
                exit(EXIT_SUCCESS);
            }
        },
        "version",
        "Show version of Unity System Compositor"));

    version_flag(*config);

    miral::CommandLineOption blacklist_flag{
        &usc::validate_video_driver,
        "blacklist", "Video driver blacklist regex to use", ""
    };
    blacklist_flag(*config);

    // socket file settings
    std::string socket_file;
    bool no_socket_file;
    bool public_socket;

    usc::ExtractSocketConfig extract_socket_config{
        [&](std::string file_flag, bool no_file_flag)
        {
            socket_file = file_flag;
            no_socket_file = no_file_flag;
        }
    };
    extract_socket_config(*config);

    auto public_socket_flag = miral::pre_init(miral::CommandLineOption(
        [&](bool is_set)
        {
            public_socket = is_set && !no_socket_file;
        },
        "public-socket",
        "Make the socket file publicly writable",
        true));
    public_socket_flag(*config);

    // spinner+session switcher settings
    std::shared_ptr<usc::SessionSwitcher> session_switcher;
    auto spinner_flag = miral::pre_init(miral::CommandLineOption(
        [&](std::string const& spinner_path)
        {
            auto spinner = std::make_shared<usc::ExternalSpinner>(
                               spinner_path,
                               socket_file);

            session_switcher = std::make_shared<usc::SessionSwitcher>(spinner);
        },
        "spinner",
        "Path to spinner executable",
        ""));
    spinner_flag(*config);

    // hardware-cursor settings
    bool enable_hardware_cursor;
    auto enable_hardware_cursor_flag = miral::pre_init(miral::CommandLineOption(
        [&](bool is_set)
        {
            enable_hardware_cursor = is_set;
        },
        "enable-hardware-cursor",
        "Enable the hardware cursor (disabled by default)",
        false));
    enable_hardware_cursor_flag(*config);

    miral::display_configuration_options(*config);

    config->set_command_line(argc, const_cast<char const **>(argv));

    config->set_command_line_handler(&ignore_unknown_arguments);

    config->wrap_cursor_listener([&](std::shared_ptr<mir::input::CursorListener> const& default_)
        -> std::shared_ptr<mir::input::CursorListener>
        {
            // This is a workaround for u8 desktop preview in 14.04 for the lack of client cursor API.
            // We need to disable the cursor for XMir but leave it on for the desktop preview.
            // Luckily as it stands they run inside seperate instances of USC. ~racarr
            if (enable_hardware_cursor)
                return default_;
            else
                return std::make_shared<NullCursorListener>();
        });

    config->override_the_server_status_listener([&]()
        -> std::shared_ptr<mir::ServerStatusListener>
        {
            return std::make_shared<ServerStatusListener>(config->the_focus_controller());
        });

    config->override_the_window_manager_builder(
        [&](mir::shell::FocusController* focus_controller)
        {
            return std::make_shared<usc::WindowManager>(
                focus_controller,
                config->the_shell_display_layout(),
                config->the_session_coordinator(),
                session_switcher);
        });

    config->override_the_cookie_authority([]()
        -> std::shared_ptr<mir::cookie::Authority>
        {
            return std::make_unique<StubCookieAuthority>();
        });

    config->set_config_filename("unity-system-compositor.conf");

    // settings for the display manager connection
    mir::optional_value<int> from_dm_fd;
    mir::optional_value<int> to_dm_fd;
    std::string debug_active_session_name;

    auto from_dm_fd_flag = miral::pre_init(miral::CommandLineOption(
        [&](mir::optional_value<int> const& from_dm_file_descriptor)
        {
            from_dm_fd = from_dm_file_descriptor;
        },
        "from-dm-fd",
        "File descriptor of read end of pipe from display manager [int]"));
    from_dm_fd_flag(*config);

    auto to_dm_fd_flag = miral::pre_init(miral::CommandLineOption(
        [&](mir::optional_value<int> const& to_dm_file_descriptor)
        {
            to_dm_fd = to_dm_file_descriptor;
        },
        "to-dm-fd",
        "File descriptor of write end of pipe to display manager [int]"));
    to_dm_fd_flag(*config);

    auto display_stub_flag = miral::pre_init(miral::CommandLineOption(
        [&](std::string const& display_stub_name)
        {
            debug_active_session_name = display_stub_name;
        },
        "debug-active-session-name",
        "Expected connection when run without a display manager (only useful when debugging)",
        "nested-mir@:/run/user/1000/mir_socket"));
    display_stub_flag(*config);

    // the display manager connection
    std::shared_ptr<usc::DMConnection> dm_connection;

    miral::CommandLineOption debug_without_dm_flag{
        [&](bool debug_without_dm)
        {
            dm_connection = create_dm_connection(
                                from_dm_fd,
                                to_dm_fd,
                                debug_active_session_name,
                                debug_without_dm,
                                session_switcher);

            // Make socket world-writable, since users need to talk to us.  No worries
            // about race condition, since we are adding permissions, not restricting
            // them.
            if (public_socket && chmod(socket_file.c_str(), 0777) == -1)
                std::cerr << "Unable to chmod socket file " << socket_file << ": " << strerror(errno) << std::endl;

            dm_connection->start();
        },
        "debug-without-dm",
        "Run without a display manager (only useful when debugging)"
    };
    debug_without_dm_flag(*config);

    config->apply_settings();

    usc::SystemCompositor system_compositor{};
    system_compositor(*config);

    config->run();

    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
