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
class SessionSwitcher;
class Screen;
class UnityDisplayService;
class PowerButtonEventSink;
class UserActivityEventSink;
class InputConfiguration;
class UnityInputService;
class DBusConnectionThread;
class DBusEventLoop;
class Clock;

class Server : private mir::Server
{
public:
    explicit Server(int argc, char** argv,
        std::shared_ptr<SessionSwitcher>& session_switcher);

    using mir::Server::add_init_callback;
    using mir::Server::get_options;
    using mir::Server::run;
    using mir::Server::the_main_loop;
    using mir::Server::the_composite_event_filter;
    using mir::Server::the_display;
    using mir::Server::the_compositor;
    using mir::Server::the_touch_visualizer;

    virtual std::shared_ptr<Screen> the_screen();
    virtual std::shared_ptr<InputConfiguration> the_input_configuration();
    virtual std::shared_ptr<mir::input::EventFilter> the_screen_event_handler();
    virtual std::shared_ptr<UnityDisplayService> the_unity_display_service();
    virtual std::shared_ptr<UnityInputService> the_unity_input_service();
    virtual std::shared_ptr<PowerButtonEventSink> the_power_button_event_sink();
    virtual std::shared_ptr<UserActivityEventSink> the_user_activity_event_sink();
    virtual std::shared_ptr<DBusEventLoop> the_dbus_event_loop();
    virtual std::shared_ptr<DBusConnectionThread> the_dbus_connection_thread();
    virtual std::shared_ptr<Clock> the_clock();

private:
    std::string dbus_bus_address();

    mir::CachedPtr<Screen> screen;
    mir::CachedPtr<InputConfiguration> input_configuration;
    mir::CachedPtr<mir::input::EventFilter> screen_event_handler;
    mir::CachedPtr<DBusConnectionThread> dbus_thread;
    mir::CachedPtr<DBusEventLoop> dbus_loop;
    mir::CachedPtr<UnityDisplayService> unity_display_service;
    mir::CachedPtr<PowerButtonEventSink> power_button_event_sink;
    mir::CachedPtr<UserActivityEventSink> user_activity_event_sink;
    mir::CachedPtr<UnityInputService> unity_input_service;
    mir::CachedPtr<Clock> clock;
};

}

#endif
