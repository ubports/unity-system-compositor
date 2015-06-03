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

#include "session_monitor.h"

#include "mir/geometry/rectangle.h"
#include "mir/shell/surface_ready_observer.h"
#include "mir/scene/session.h"
#include "mir/scene/session_coordinator.h"
#include "mir/scene/surface.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir/shell/display_layout.h"
#include "mir/shell/focus_controller.h"
#include "mir/shell/surface_specification.h"

#include "mir_toolkit/client_types.h"

#include <iostream>

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;

namespace
{

class UscSession : public usc::Session
{
public:
    UscSession(
        std::shared_ptr<ms::Session> const& scene_session,
        msh::FocusController& focus_controller)
        : scene_session{scene_session},
          focus_controller(focus_controller)
    {
    }

    std::string name()
    {
        return scene_session->name();
    }

    void show() override
    {
        scene_session->show();
    }

    void hide() override
    {
        scene_session->hide();
    }

    void raise_and_focus() override
    {
        auto const surface = scene_session->default_surface();
        focus_controller.raise({surface});
        focus_controller.set_focus_to(scene_session, surface);
    }

    bool corresponds_to(mir::frontend::Session const* s) override
    {
        return scene_session.get() == s;
    }

    std::shared_ptr<ms::Session> const scene_session;
    msh::FocusController& focus_controller;
};
}

usc::WindowManager::WindowManager(
    mir::shell::FocusController* focus_controller,
    std::shared_ptr<mir::shell::DisplayLayout> const& display_layout,
    std::shared_ptr<ms::SessionCoordinator> const& session_coordinator,
    std::shared_ptr<SessionMonitor> const& session_monitor) :
    focus_controller{focus_controller},
    display_layout{display_layout},
    session_coordinator{session_coordinator},
    session_monitor{session_monitor}
{
}

void usc::WindowManager::add_session(std::shared_ptr<ms::Session> const& session)
{
    std::cerr << "Opening session " << session->name() << std::endl;

    focus_controller->set_focus_to(session, {});

    auto const usc_session = std::make_shared<UscSession>(session, *focus_controller);

    session_monitor->add(usc_session, session->process_id());
}

void usc::WindowManager::remove_session(std::shared_ptr<ms::Session> const& session)
{
    std::cerr << "Closing session " << session->name() << std::endl;

    auto const next_session = session_coordinator->successor_of({});
    if (next_session)
        focus_controller->set_focus_to(next_session, next_session->default_surface());
    else
        focus_controller->set_focus_to(next_session, {});

    session_monitor->remove(session);
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

    auto const session_ready_observer = std::make_shared<msh::SurfaceReadyObserver>(
        [this](std::shared_ptr<ms::Session> const& session, std::shared_ptr<ms::Surface> const& surface)
        {
            session_monitor->mark_ready(session.get());
        },
        session,
        surface);

    surface->add_observer(session_ready_observer);

    return result;
}

void usc::WindowManager::modify_surface(
    std::shared_ptr<ms::Session> const& /*session*/,
    std::shared_ptr<ms::Surface> const& surface,
    msh::SurfaceSpecification const& modifications)
{
    if (modifications.name.is_set())
        surface->rename(modifications.name.value());
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

bool usc::WindowManager::handle_keyboard_event(MirKeyboardEvent const* /*event*/)
{
    return false;
}

bool usc::WindowManager::handle_touch_event(MirTouchEvent const* /*event*/)
{
    return false;
}

bool usc::WindowManager::handle_pointer_event(MirPointerEvent const* /*event*/)
{
    return false;
}

int usc::WindowManager::set_surface_attribute(
    std::shared_ptr<ms::Session> const& /*session*/,
    std::shared_ptr<ms::Surface> const& surface,
    MirSurfaceAttrib attrib,
    int value)
{
    return surface->configure(attrib, value);
}
