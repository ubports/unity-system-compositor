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

#include <thread>

namespace ms = mir::scene;
namespace msh = mir::shell;

namespace
{

struct OnFramePosted : ms::NullSurfaceObserver
{
    OnFramePosted(std::function<void()> const& action)
        : action{action}
    {
    }

    void frame_posted(int) override
    {
        if (posted == 0)
        {
            /* TODO: Fix this workaround for SurfaceObserver recursive deadlock */
            std::thread{action}.detach();
            ++posted;
        }
    }

    int posted = 0;
    std::function<void()> const action;
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

    surface->add_observer(
        std::make_shared<OnFramePosted>(
            [this, session] { session_switcher->mark_ready(session); }));

    return surface;
}
