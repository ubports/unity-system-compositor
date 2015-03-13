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

#include "shell.h"
#include "session_switcher.h"
#include "window_manager.h"

#include <mir/scene/null_surface_observer.h>
#include <mir/scene/session.h>
#include <mir/scene/surface.h>
#include <mir/frontend/session.h>

#include <iostream>

namespace msh = mir::shell;
namespace ms = mir::scene;
namespace mf = mir::frontend;


usc::Shell::Shell(
    std::shared_ptr<mir::shell::InputTargeter> const& input_targeter,
    std::shared_ptr<mir::scene::SurfaceCoordinator> const& surface_coordinator,
    std::shared_ptr<mir::scene::SessionCoordinator> const& session_coordinator,
    std::shared_ptr<mir::scene::PromptSessionManager> const& prompt_session_manager,
    std::shared_ptr<mir::scene::SurfaceConfigurator> const& surface_configurator,
    std::shared_ptr<mir::shell::DisplayLayout> const& display_layout,
    std::shared_ptr<SessionSwitcher> const& session_switcher)
    : msh::AbstractShell(input_targeter, surface_coordinator, session_coordinator, prompt_session_manager,
      [&](msh::FocusController* focus_controller)
      {
        return std::make_shared<WindowManager>(
            focus_controller,
            display_layout,
            session_coordinator,
            surface_configurator,
            session_switcher);
      })
{
}

std::shared_ptr<ms::Session>
usc::Shell::open_session(
    pid_t client_pid,
    std::string const& name,
    std::shared_ptr<mf::EventSink> const& sink)
{
    auto orig = msh::AbstractShell::open_session(client_pid, name, sink);
    return orig;
}

void usc::Shell::close_session(std::shared_ptr<ms::Session> const& session)
{
    msh::AbstractShell::close_session(session);
}

mf::SurfaceId usc::Shell::create_surface(std::shared_ptr<ms::Session> const& session, ms::SurfaceCreationParameters const& params)
{
    auto const id = msh::AbstractShell::create_surface(session, params);


    return id;
}
