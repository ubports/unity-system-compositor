/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef USC_WINDOW_MANAGER_H_
#define USC_WINDOW_MANAGER_H_

#include <mir/shell/system_compositor_window_manager.h>
#include "session_monitor.h"

namespace mir
{
namespace scene { class PlacementStrategy; class SessionCoordinator; }
namespace shell { class FocusController; class DisplayLayout; }
}

namespace usc
{
class SessionMonitor;

class WindowManager : public mir::shell::SystemCompositorWindowManager
{
public:
    explicit WindowManager(
        mir::shell::FocusController* focus_controller,
        std::shared_ptr<mir::shell::DisplayLayout> const& display_layout,
        std::shared_ptr<mir::scene::SessionCoordinator> const& session_coordinator,
        std::shared_ptr<SessionMonitor> const& session_switcher);

private:
    std::shared_ptr<SessionMonitor> const session_monitor;

    virtual void on_session_added(std::shared_ptr<mir::scene::Session> const& session) const;
    virtual void on_session_removed(std::shared_ptr<mir::scene::Session> const& session) const;
    virtual void on_session_ready(std::shared_ptr<mir::scene::Session> const& session) const;
};
}

#endif /* USC_WINDOW_MANAGER_H_ */
