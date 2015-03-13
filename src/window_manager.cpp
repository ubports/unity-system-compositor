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

#include "window_manager.h"

#include "mir/geometry/rectangle.h"
#include "mir/scene/null_surface_observer.h"
#include "mir/scene/session.h"
#include "mir/scene/session_coordinator.h"
#include "mir/scene/surface.h"
#include "mir/scene/surface_configurator.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir/shell/display_layout.h"
#include "mir/shell/focus_controller.h"

#include "mir_toolkit/client_types.h"

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;

usc::WindowManager::WindowManager(
    mir::shell::FocusController* focus_controller,
    std::shared_ptr<mir::shell::DisplayLayout> const& display_layout,
    std::shared_ptr<mir::scene::SessionCoordinator> const& session_coordinator,
    std::shared_ptr<mir::scene::SurfaceConfigurator> const& surface_configurator) :
    focus_controller{focus_controller},
    display_layout{display_layout},
    session_coordinator{session_coordinator},
    surface_configurator{surface_configurator}
{
}

void usc::WindowManager::add_session(std::shared_ptr<ms::Session> const& session)
{
    focus_controller->set_focus_to(session, {});
}

void usc::WindowManager::remove_session(std::shared_ptr<ms::Session> const& /*session*/)
{
    auto const next_session = session_coordinator->successor_of({});
    if (next_session)
        focus_controller->set_focus_to(next_session, next_session->default_surface());
    else
        focus_controller->set_focus_to(next_session, {});
}

namespace
{
class SurfaceReadyObserver : public ms::NullSurfaceObserver,
    public std::enable_shared_from_this<SurfaceReadyObserver>
{
public:
    SurfaceReadyObserver(
        msh::FocusController* const focus_controller,
        std::shared_ptr<ms::Session> const& session,
        std::shared_ptr<ms::Surface> const& surface) :
        focus_controller{focus_controller},
        session{session},
        surface{surface}
    {
    }

private:
    void frame_posted(int) override
    {
        if (auto const s = surface.lock())
        {
            focus_controller->set_focus_to(session.lock(), s);
            s->remove_observer(shared_from_this());
        }
    }

    msh::FocusController* const focus_controller;
    std::weak_ptr<ms::Session> const session;
    std::weak_ptr<ms::Surface> const surface;
};
}

auto usc::WindowManager::add_surface(
    std::shared_ptr<ms::Session> const& session,
    ms::SurfaceCreationParameters const& params,
    std::function<mf::SurfaceId(std::shared_ptr<ms::Session> const& session, ms::SurfaceCreationParameters const& params)> const& build)
-> mf::SurfaceId
{
    mir::graphics::DisplayConfigurationOutputId const output_id_invalid{
        mir_display_output_id_invalid};
    auto placed_parameters = params;

    mir::geometry::Rectangle rect{params.top_left, params.size};

    if (params.output_id != output_id_invalid)
    {
        display_layout->place_in_output(params.output_id, rect);
    }

    placed_parameters.top_left = rect.top_left;
    placed_parameters.size = rect.size;

    auto const result = build(session, placed_parameters);
    auto const surface = session->surface(result);

    surface->add_observer(std::make_shared<SurfaceReadyObserver>(focus_controller, session, surface));

    return result;
}

void usc::WindowManager::remove_surface(
    std::shared_ptr<ms::Session> const& /*session*/,
    std::weak_ptr<ms::Surface> const& /*surface*/)
{
}

void usc::WindowManager::add_display(mir::geometry::Rectangle const& /*area*/)
{
}

void usc::WindowManager::remove_display(mir::geometry::Rectangle const& /*area*/)
{
}

bool usc::WindowManager::handle_key_event(MirKeyInputEvent const* /*event*/)
{
    return false;
}

bool usc::WindowManager::handle_touch_event(MirTouchInputEvent const* /*event*/)
{
    return false;
}

bool usc::WindowManager::handle_pointer_event(MirPointerInputEvent const* /*event*/)
{
    return false;
}

int usc::WindowManager::set_surface_attribute(
    std::shared_ptr<ms::Session> const& /*session*/,
    std::shared_ptr<ms::Surface> const& surface,
    MirSurfaceAttrib attrib,
    int value)
{
    auto const configured_value = surface_configurator->select_attribute_value(*surface, attrib, value);

    auto const result = surface->configure(attrib, configured_value);

    surface_configurator->attribute_set(*surface, attrib, result);

    return result;
}
