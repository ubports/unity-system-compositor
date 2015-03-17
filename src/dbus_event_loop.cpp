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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "dbus_event_loop.h"

#include <algorithm>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <fcntl.h>

namespace
{

uint32_t dbus_flags_to_epoll_events(DBusWatch* bus_watch)
{
    unsigned int flags;
    uint32_t events{0};

    if (!dbus_watch_get_enabled(bus_watch))
        return 0;

    flags = dbus_watch_get_flags(bus_watch);
    if (flags & DBUS_WATCH_READABLE)
        events |= EPOLLIN;
    if (flags & DBUS_WATCH_WRITABLE)
        events |= EPOLLOUT;

    return events | EPOLLHUP | EPOLLERR;
}

unsigned int epoll_events_to_dbus_flags(uint32_t events)
{
    unsigned int flags{0};

    if (events & EPOLLIN)
        flags |= DBUS_WATCH_READABLE;
    if (events & EPOLLOUT)
        flags |= DBUS_WATCH_WRITABLE;
    if (events & EPOLLHUP)
        flags |= DBUS_WATCH_HANGUP;
    if (events & EPOLLERR)
        flags |= DBUS_WATCH_ERROR;

    return flags;
}

timespec msec_to_timespec(int msec)
{
    static long const milli_to_nano = 1000000;
    time_t const sec = msec / 1000;
    long const nsec = (msec % 1000) * milli_to_nano;
    return timespec{sec, nsec};
}

}

usc::DBusEventLoop::DBusEventLoop(DBusConnection* connection)
    : connection{connection},
      running{false},
      epoll_fd{epoll_create1(EPOLL_CLOEXEC)}
{
    if (epoll_fd == -1)
        throw std::system_error(errno, std::system_category(), "DBusEventLoop epoll_create1");

    int pipefd[2]{};

    if (pipe2(pipefd, O_CLOEXEC) == -1)
        throw std::system_error(errno, std::system_category(), "DBusEventLoop pipe");

    wake_up_fd_r = mir::Fd{pipefd[0]};
    wake_up_fd_w = mir::Fd{pipefd[1]};

    epoll_event ev{};
    ev.data.fd = wake_up_fd_r;
    ev.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1)
    {
        throw std::system_error(errno, std::system_category(),
                                "DBusEventLoop epoll_ctl add wake up");
    }

    dbus_connection_set_watch_functions(
        connection,
        DBusEventLoop::static_add_watch,
        DBusEventLoop::static_remove_watch,
        DBusEventLoop::static_toggle_watch,
        this,
        nullptr);

    dbus_connection_set_timeout_functions(
        connection,
        DBusEventLoop::static_add_timeout,
        DBusEventLoop::static_remove_timeout,
        DBusEventLoop::static_toggle_timeout,
        this,
        nullptr);

    dbus_connection_set_wakeup_main_function(
        connection,
        DBusEventLoop::static_wake_up_loop,
        this, nullptr);
}

usc::DBusEventLoop::~DBusEventLoop()
{
    stop();

    dbus_connection_set_watch_functions(
        connection, nullptr, nullptr, nullptr, nullptr, nullptr);

    dbus_connection_set_timeout_functions(
        connection, nullptr, nullptr, nullptr, nullptr, nullptr);

    dbus_connection_set_wakeup_main_function(
        connection, nullptr, nullptr, nullptr);
}

void usc::DBusEventLoop::run(std::promise<void>& started)
{
    running = true;
    started.set_value();

    while (running)
    {
        epoll_event event{};
        int n = epoll_wait(epoll_fd, &event, 1, -1);
        if (n == -1)
        {
            if (errno == EINTR)
                continue;

            throw std::system_error(errno, std::system_category(), "DBusEventLoop epoll_wait");
        }

        if (event.data.fd == wake_up_fd_r)
        {
            char c;
            if (read(event.data.fd, &c, 1));
        }
        else
        {
            auto const& matching_watches = enabled_watches_for(event.data.fd);
            for (auto const& watch : matching_watches)
            {
                dbus_watch_handle(watch, epoll_events_to_dbus_flags(event.events));
            }

            if (matching_watches.empty())
            {
                auto timeout = enabled_timeout_for(event.data.fd);
                if (timeout)
                    dbus_timeout_handle(timeout);
            }
        }

        dispatch_actions();

        dbus_connection_flush(connection);

        while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS)
            continue;
    }

    // Flush any remaining outgoing messages
    dbus_connection_flush(connection);
}

void usc::DBusEventLoop::stop()
{
    running = false;
    wake_up_loop();
}

std::vector<DBusWatch*> usc::DBusEventLoop::enabled_watches_for(int fd)
{
    std::lock_guard<std::mutex> lock{mutex};

    std::vector<DBusWatch*> ret_watches;

    for (auto const& w : watches)
    {
        if (dbus_watch_get_unix_fd(w) == fd &&
            dbus_watch_get_enabled(w))
        {
            ret_watches.push_back(w);
        }
    }

    return ret_watches;
}

DBusTimeout* usc::DBusEventLoop::enabled_timeout_for(int fd)
{
    std::lock_guard<std::mutex> lock{mutex};

    for (auto const& p : timeouts)
    {
        if (p.second == fd && dbus_timeout_get_enabled(p.first))
            return p.first;
    }

    return nullptr;
}

dbus_bool_t usc::DBusEventLoop::add_watch(DBusWatch* watch)
{
    std::lock_guard<std::mutex> lock{mutex};

    int const watch_fd = dbus_watch_get_unix_fd(watch);

    if (!is_watched(watch_fd))
    {
        epoll_event ev{};
        ev.data.fd = watch_fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, watch_fd, &ev) == -1)
            return FALSE;
    }

    watches.push_back(watch);

    update_events_for_watch_fd(watch_fd);

    return TRUE;
}

void usc::DBusEventLoop::remove_watch(DBusWatch* watch)
{
    std::lock_guard<std::mutex> lock{mutex};

    watches.erase(std::remove(begin(watches), end(watches), watch), end(watches));

    int const watch_fd = dbus_watch_get_unix_fd(watch);
    if (!is_watched(watch_fd))
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, watch_fd, nullptr);
    else
        update_events_for_watch_fd(watch_fd);
}

void usc::DBusEventLoop::toggle_watch(DBusWatch* watch)
{
    std::lock_guard<std::mutex> lock{mutex};

    int const watch_fd = dbus_watch_get_unix_fd(watch);
    update_events_for_watch_fd(watch_fd);
}

void usc::DBusEventLoop::update_events_for_watch_fd(int watch_fd)
{
    epoll_event ev{};
    ev.events = epoll_events_for_watch_fd(watch_fd);
    ev.data.fd = watch_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, watch_fd, &ev);
}

bool usc::DBusEventLoop::is_watched(int watch_fd)
{
    auto const iter = std::find_if(
        begin(watches), end(watches),
        [watch_fd] (DBusWatch* w) { return dbus_watch_get_unix_fd(w) == watch_fd; });

    return iter != end(watches);
}

uint32_t usc::DBusEventLoop::epoll_events_for_watch_fd(int fd)
{
    uint32_t events{};

    for (auto const& watch : watches)
    {
        if (dbus_watch_get_unix_fd(watch) == fd)
            events |= dbus_flags_to_epoll_events(watch);
    }

    return events;
}

dbus_bool_t usc::DBusEventLoop::add_timeout(DBusTimeout* timeout)
{
    std::lock_guard<std::mutex> lock{mutex};

    auto tfd = mir::Fd{timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC)};

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = tfd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tfd, &ev))
        return FALSE;

    timeouts.emplace_back(timeout, std::move(tfd));

    return update_timer_fd_for(timeout);
}

void usc::DBusEventLoop::remove_timeout(DBusTimeout* timeout)
{
    std::lock_guard<std::mutex> lock{mutex};

    auto tfd = timer_fd_for(timeout);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, tfd, nullptr);

    timeouts.erase(
        std::remove_if(begin(timeouts), end(timeouts),
            [timeout] (decltype(timeouts)::const_reference p)
            {
                return p.first == timeout;
            }),
        end(timeouts));

}

void usc::DBusEventLoop::toggle_timeout(DBusTimeout* timeout)
{
    std::lock_guard<std::mutex> lock{mutex};
    update_timer_fd_for(timeout);
}


dbus_bool_t usc::DBusEventLoop::update_timer_fd_for(DBusTimeout* timeout)
{
    itimerspec spec{};

    auto tfd = timer_fd_for(timeout);

    if (dbus_timeout_get_enabled(timeout))
    {
        spec.it_interval = msec_to_timespec(dbus_timeout_get_interval(timeout));
        spec.it_value = spec.it_interval;
    }

    if (timerfd_settime(tfd, 0, &spec, nullptr) == -1)
        return FALSE;

    return TRUE;
}

int usc::DBusEventLoop::timer_fd_for(DBusTimeout* timeout)
{
    auto iter = std::find_if(begin(timeouts), end(timeouts),
        [timeout] (std::pair<DBusTimeout*,int> const& p)
        {
            return p.first == timeout;
        });
    return (iter == end(timeouts) ? -1 : iter->second);
}

void usc::DBusEventLoop::wake_up_loop()
{
    if (write(wake_up_fd_w, "a", 1) != 1)
        throw std::system_error(errno, std::system_category(), "DBusEventLoop write wake up");
}

void usc::DBusEventLoop::enqueue(std::function<void()> const& action)
{
    {
        std::lock_guard<std::mutex> lock{mutex};
        actions.push_back(action);
    }

    wake_up_loop();
}

void usc::DBusEventLoop::dispatch_actions()
{
    decltype(actions) actions_to_dispatch;

    {
        std::lock_guard<std::mutex> lock{mutex};
        actions.swap(actions_to_dispatch);
    }

    for (auto const& action : actions_to_dispatch)
        action();
}

dbus_bool_t usc::DBusEventLoop::static_add_watch(DBusWatch* watch, void* data)
{
    return static_cast<DBusEventLoop*>(data)->add_watch(watch);
}

void usc::DBusEventLoop::static_remove_watch(DBusWatch* watch, void* data)
{
    static_cast<DBusEventLoop*>(data)->remove_watch(watch);
}

void usc::DBusEventLoop::static_toggle_watch(DBusWatch* watch, void* data)
{
    static_cast<DBusEventLoop*>(data)->toggle_watch(watch);
}

dbus_bool_t usc::DBusEventLoop::static_add_timeout(DBusTimeout* timeout, void* data)
{
    return static_cast<DBusEventLoop*>(data)->add_timeout(timeout);
}

void usc::DBusEventLoop::static_remove_timeout(DBusTimeout* timeout, void* data)
{
    static_cast<DBusEventLoop*>(data)->remove_timeout(timeout);
}

void usc::DBusEventLoop::static_toggle_timeout(DBusTimeout* timeout, void* data)
{
    static_cast<DBusEventLoop*>(data)->toggle_timeout(timeout);
}

void usc::DBusEventLoop::static_wake_up_loop(void* data)
{
    static_cast<DBusEventLoop*>(data)->wake_up_loop();
}
