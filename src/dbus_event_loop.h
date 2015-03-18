/*
 * Copyright © 2015 Canonical Ltd.
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

#ifndef USC_DBUS_EVENT_LOOP_H_
#define USC_DBUS_EVENT_LOOP_H_

#include <mir/fd.h>

#include <dbus/dbus.h>

#include <atomic>
#include <vector>
#include <mutex>
#include <future>

namespace usc
{

class DBusEventLoop
{
public:
    DBusEventLoop(DBusConnection* connection);
    ~DBusEventLoop();

    void run(std::promise<void>& started);
    void stop();

    void enqueue(std::function<void()> const& action);

private:
    std::vector<DBusWatch*> enabled_watches_for(int fd);
    DBusTimeout* enabled_timeout_for(int fd);

    dbus_bool_t add_watch(DBusWatch* watch);
    void remove_watch(DBusWatch* watch);
    void toggle_watch(DBusWatch* watch);
    void update_events_for_watch_fd(int watch_fd);
    bool is_watched(int watch_fd);
    uint32_t epoll_events_for_watch_fd(int watch_fd);

    dbus_bool_t add_timeout(DBusTimeout* timeout);
    void remove_timeout(DBusTimeout* timeout);
    void toggle_timeout(DBusTimeout* timeout);
    dbus_bool_t update_timer_fd_for(DBusTimeout* timeout);
    int timer_fd_for(DBusTimeout* timeout);

    void wake_up_loop();
    void dispatch_actions();

    static dbus_bool_t static_add_watch(DBusWatch* watch, void* data);
    static void static_remove_watch(DBusWatch* watch, void* data);
    static void static_toggle_watch(DBusWatch* watch, void* data);
    static dbus_bool_t static_add_timeout(DBusTimeout* timeout, void* data);
    static void static_remove_timeout(DBusTimeout* timeout, void* data);
    static void static_toggle_timeout(DBusTimeout* timeout, void* data);
    static void static_wake_up_loop(void* data);

    DBusConnection* const connection;
    std::atomic<bool> running;

    std::mutex mutex;
    std::vector<DBusWatch*> watches;
    std::vector<std::pair<DBusTimeout*,mir::Fd>> timeouts;
    std::vector<std::function<void(void)>> actions;
    mir::Fd epoll_fd;
    mir::Fd wake_up_fd_r;
    mir::Fd wake_up_fd_w;
};

}

#endif
