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

#include "surface_coordinator.h"
#include "session_switcher.h"

#include <mir/scene/null_surface_observer.h>
#include <mir/scene/surface.h>

namespace ms = mir::scene;
namespace msh = mir::shell;

namespace
{

struct SessionReadyObserver : ms::NullSurfaceObserver,
                              std::enable_shared_from_this<SessionReadyObserver>
{
    SessionReadyObserver(
        std::shared_ptr<usc::SessionSwitcher> const& switcher,
        std::shared_ptr<ms::Surface> const& surface,
        ms::Session const* session)
        : switcher{switcher},
          surface{surface},
          session{session}
    {
    }

    void frame_posted(int) override
    {
        ++num_frames_posted;
        if (num_frames_posted == num_frames_for_session_ready)
        {
            switcher->mark_ready(session);
            surface->remove_observer(shared_from_this());
        }
    }

    std::shared_ptr<usc::SessionSwitcher> const switcher;
    std::shared_ptr<ms::Surface> const surface;
    ms::Session const* const session;
    // We need to wait for the second frame before marking the session
    // as ready. The first frame posted from sessions is a blank frame.
    // TODO: Solve this issue at its root and remove this workaround
    int const num_frames_for_session_ready{2};
    int num_frames_posted{0};
};

}

usc::SurfaceCoordinator::SurfaceCoordinator(
    std::shared_ptr<ms::SurfaceCoordinator> const& wrapped,
    std::shared_ptr<SessionSwitcher> const& session_switcher)
    : msh::SurfaceCoordinatorWrapper{wrapped},
      session_switcher{session_switcher}
{
}

std::shared_ptr<ms::Surface> usc::SurfaceCoordinator::add_surface(
    ms::SurfaceCreationParameters const& params,
    ms::Session* session)
{
    auto const surface = msh::SurfaceCoordinatorWrapper::add_surface(params, session);

    auto const session_ready_observer = std::make_shared<SessionReadyObserver>(
        session_switcher, surface, session);

    surface->add_observer(session_ready_observer);

    return surface;
}
