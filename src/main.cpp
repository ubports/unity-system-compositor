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

#include "external_spinner.h"
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

namespace usc { class SessionSwitcher; }

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

const char* const dm_from_fd = "from-dm-fd";
const char* const dm_to_fd = "to-dm-fd";
const char* const dm_stub = "debug-without-dm";
const char* const dm_stub_active = "debug-active-session-name";

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

    config->add_configuration_option(dm_from_fd, "File descriptor of read end of pipe from display manager [int]",
        mir::OptionType::integer);
    config->add_configuration_option(dm_to_fd, "File descriptor of write end of pipe to display manager [int]",
        mir::OptionType::integer);
    config->add_configuration_option(dm_stub, "Run without a display manager (only useful when debugging)", mir::OptionType::null);
    config->add_configuration_option(dm_stub_active, "Expected connection when run without a display manager (only useful when debugging)", "nested-mir@:/run/user/1000/mir_socket");
    config->add_configuration_option("spinner", "Path to spinner executable",  mir::OptionType::string);
    config->add_configuration_option("public-socket", "Make the socket file publicly writable",  mir::OptionType::boolean);
    config->add_configuration_option("enable-hardware-cursor", "Enable the hardware cursor (disabled by default)",  mir::OptionType::boolean);
    miral::display_configuration_options(*config);

    config->set_command_line(argc, const_cast<char const **>(argv));

    config->set_command_line_handler(&ignore_unknown_arguments);

    config->wrap_cursor_listener([&](std::shared_ptr<mir::input::CursorListener> const& default_)
        -> std::shared_ptr<mir::input::CursorListener>
        {
            // This is a workaround for u8 desktop preview in 14.04 for the lack of client cursor API.
            // We need to disable the cursor for XMir but leave it on for the desktop preview.
            // Luckily as it stands they run inside seperate instances of USC. ~racarr
            if (config->get_options()->get("enable-hardware-cursor", false))
                return default_;
            else
                return std::make_shared<NullCursorListener>();
        });

    config->override_the_server_status_listener([&]()
        -> std::shared_ptr<mir::ServerStatusListener>
        {
            return std::make_shared<ServerStatusListener>(config->the_focus_controller());
        });

    std::shared_ptr<usc::SessionSwitcher> session_switcher;

    config->override_the_window_manager_builder(
        [&](mir::shell::FocusController* focus_controller)
        {
            auto spinner = std::make_shared<usc::ExternalSpinner>(
                               config->get_options()->get("spinner", ""),
                               config->get_options()->get("file", "/tmp/mir_socket"));

            session_switcher = std::make_shared<usc::SessionSwitcher>(spinner);

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

    config->apply_settings();

    auto the_session_switcher = [&session_switcher]() { return session_switcher; };

    usc::SystemCompositor system_compositor{the_session_switcher};
    system_compositor(*config);

    config->run();

    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
