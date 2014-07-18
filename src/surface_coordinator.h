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

#ifndef USC_SURFACE_COORDINATOR_H_
#define USC_SURFACE_COORDINATOR_H_

#include <mir/shell/surface_coordinator_wrapper.h>

#include <memory>

namespace usc
{
class SessionSwitcher;

class SurfaceCoordinator : public mir::shell::SurfaceCoordinatorWrapper
{
public:
    SurfaceCoordinator(
        std::shared_ptr<mir::scene::SurfaceCoordinator> const& wrapped,
        std::shared_ptr<SessionSwitcher> const& session_switcher);

private:
    std::shared_ptr<mir::scene::Surface> add_surface(
        mir::scene::SurfaceCreationParameters const& params,
        mir::scene::Session* session) override;

    std::shared_ptr<SessionSwitcher> const session_switcher;
};

}

#endif
