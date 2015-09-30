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

#ifndef USC_SERVER_H_
#define USC_SERVER_H_

#include <mir/server.h>
#include <mir/cached_ptr.h>
#include <mir/options/option.h>

#include <chrono>

namespace mir
{
namespace input
{
class EventFilter;
}
}

namespace usc
{
class Spinner;
class SessionSwitcher;
class DMMessageHandler;
class DMConnection;
class Screen;
class ScreenHardware;
class UnityScreenService;
class InputConfiguration;
class UnityInputService;
class DBusConnectionThread;
class Clock;

class Server : private mir::Server
{
public:
    explicit Server(int argc, char** argv);

    using mir::Server::add_init_callback;
    using mir::Server::run;
    using mir::Server::the_main_loop;
    using mir::Server::the_composite_event_filter;
    using mir::Server::the_display;
    using mir::Server::the_compositor;
    using mir::Server::the_touch_visualizer;

    virtual std::shared_ptr<Spinner> the_spinner();
    virtual std::shared_ptr<DMMessageHandler> the_dm_message_handler();
    virtual std::shared_ptr<DMConnection> the_dm_connection();
    virtual std::shared_ptr<Screen> the_screen();
    virtual std::shared_ptr<InputConfiguration> the_input_configuration();
    virtual std::shared_ptr<mir::input::EventFilter> the_screen_event_handler();
    virtual std::shared_ptr<ScreenHardware> the_screen_hardware();
    virtual std::shared_ptr<UnityScreenService> the_unity_screen_service();
    virtual std::shared_ptr<UnityInputService> the_unity_input_service();
    virtual std::shared_ptr<DBusConnectionThread> the_dbus_connection_thread();
    virtual std::shared_ptr<Clock> the_clock();

    bool show_version()
    {
        return the_options()->is_set("version");
    }

    bool disable_inactivity_policy()
    {
        return the_options()->get("disable-inactivity-policy", false);
    }

    std::string blacklist()
    {
        auto x = the_options()->get("blacklist", "");
        return x;
    }

    bool public_socket()
    {
        return !the_options()->is_set("no-file") && the_options()->get("public-socket", true);
    }

    std::string get_socket_file()
    {
        // the_socket_file is private, so we have to re-implement it here
        return the_options()->get("file", "/tmp/mir_socket");
    }

private:
    inline auto the_options()
    -> decltype(mir::Server::get_options())
    { return mir::Server::get_options(); }

    std::chrono::milliseconds inactivity_display_off_timeout()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            seconds{the_options()->get("inactivity-display-off-timeout", 60)});
    }

    std::chrono::milliseconds inactivity_display_dim_timeout()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            seconds{the_options()->get("inactivity-display-dim-timeout", 45)});
    }

    std::chrono::milliseconds notification_display_off_timeout()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            seconds{the_options()->get("notification-display-off-timeout", 15)});
    }

    std::chrono::milliseconds notification_display_dim_timeout()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            seconds{the_options()->get("notification-display-dim-timeout", 12)});
    }

    std::chrono::milliseconds snap_decision_display_off_timeout()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            seconds{the_options()->get("snap-decision-display-off-timeout", 60)});
    }

    std::chrono::milliseconds snap_decision_display_dim_timeout()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            seconds{the_options()->get("snap-decision-display-dim-timeout", 50)});
    }

    std::chrono::milliseconds shutdown_timeout()
    {
        return std::chrono::milliseconds{
            the_options()->get("shutdown-timeout", 5000)};
    }

    std::chrono::milliseconds power_key_ignore_timeout()
    {
        return std::chrono::milliseconds{
            the_options()->get("power-key-ignore-timeout", 2000)};
    }

    bool enable_hardware_cursor()
    {
        return the_options()->get("enable-hardware-cursor", false);
    }

    std::string spinner_executable()
    {
        // TODO: once our default spinner is ready for use everywhere, replace
        // default value with DEFAULT_SPINNER instead of the empty string.
        auto x = the_options()->get("spinner", "");
        return x;
    }

    virtual std::shared_ptr<SessionSwitcher> the_session_switcher();
    std::string dbus_bus_address();

    mir::CachedPtr<Spinner> spinner;
    mir::CachedPtr<DMConnection> dm_connection;
    mir::CachedPtr<SessionSwitcher> session_switcher;
    mir::CachedPtr<Screen> screen;
    mir::CachedPtr<InputConfiguration> input_configuration;
    mir::CachedPtr<mir::input::EventFilter> screen_event_handler;
    mir::CachedPtr<ScreenHardware> screen_hardware;
    mir::CachedPtr<DBusConnectionThread> dbus_thread;
    mir::CachedPtr<UnityScreenService> unity_screen_service;
    mir::CachedPtr<UnityInputService> unity_input_service;
    mir::CachedPtr<Clock> clock;
};

}

#endif
