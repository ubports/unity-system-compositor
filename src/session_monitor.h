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
 * Authored by: Alan Griffiths <alan.griffiths@canonical.com>
 */

#ifndef USC_SESSION_MONITOR_H
#define USC_SESSION_MONITOR_H

#include <sys/types.h>

#include <memory>
#include <string>

namespace mir { namespace frontend { class Session; }};

namespace usc
{
class Session
{
public:
    virtual ~Session() = default;

    virtual std::string name() = 0;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void raise_and_focus() = 0;
    virtual bool corresponds_to(mir::frontend::Session const*) = 0;

protected:
    Session() = default;
    Session(Session const&) = delete;
    Session& operator=(Session const&) = delete;
};

class SessionMonitor
{
public:
    SessionMonitor() = default;

    virtual ~SessionMonitor() = default;

    SessionMonitor(SessionMonitor const&) = delete;

    SessionMonitor& operator=(SessionMonitor const&) = delete;

    virtual void add(std::shared_ptr<Session> const& session, pid_t pid) = 0;

    virtual void remove(
        std::shared_ptr<mir::frontend::Session> const& session) = 0;

    virtual void mark_ready(mir::frontend::Session const* session) = 0;
};
}

#endif //USC_SESSION_MONITOR_H
