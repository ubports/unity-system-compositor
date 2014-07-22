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

#ifndef USC_SESSION_COORDINATOR_H_
#define USC_SESSION_COORDINATOR_H_

#include <mir/shell/session_coordinator_wrapper.h>

#include <memory>

namespace mir
{
namespace scene { class SurfaceCoordinator; }
}

namespace usc
{
class SessionSwitcher;

class SessionCoordinator : public mir::shell::SessionCoordinatorWrapper
{
public:
    SessionCoordinator(
        std::shared_ptr<mir::scene::SessionCoordinator> const& wrapped,
        std::shared_ptr<SessionSwitcher> const& session_switcher);

private:
    std::shared_ptr<mir::frontend::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<mir::frontend::EventSink> const& sink) override;
    void close_session(std::shared_ptr<mir::frontend::Session> const& session) override;

    std::shared_ptr<SessionSwitcher> const session_switcher;
};

}

#endif
