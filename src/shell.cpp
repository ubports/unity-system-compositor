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

#include <mir/scene/null_surface_observer.h>
#include <mir/scene/session.h>
#include <mir/scene/surface.h>
#include <mir/frontend/session.h>

#include <iostream>

namespace msh = mir::shell;
namespace ms = mir::scene;
namespace mf = mir::frontend;

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
        focus_controller.set_focus_to(scene_session, scene_session->default_surface());
    }

    bool corresponds_to(mir::frontend::Session const* s) override
    {
        return scene_session.get() == s;
    }

    std::shared_ptr<ms::Session> const scene_session;
    msh::FocusController& focus_controller;
};


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

usc::Shell::Shell(
    std::shared_ptr<msh::Shell> const& wrapped,
    std::shared_ptr<SessionSwitcher> const& session_switcher)
    : msh::ShellWrapper{wrapped},
      session_switcher{session_switcher}
{
}

std::shared_ptr<ms::Session>
usc::Shell::open_session(
    pid_t client_pid,
    std::string const& name,
    std::shared_ptr<mf::EventSink> const& sink)
{
    std::cerr << "Opening session " << name << std::endl;

    auto orig = msh::ShellWrapper::open_session(client_pid, name, sink);

    auto const usc_session = std::make_shared<UscSession>(orig, *this);

    session_switcher->add(usc_session, client_pid);

    return orig;
}

void usc::Shell::close_session(std::shared_ptr<ms::Session> const& session)
{
    std::cerr << "Closing session " << session->name() << std::endl;

    msh::ShellWrapper::close_session(session);

    session_switcher->remove(session);
}

mf::SurfaceId usc::Shell::create_surface(std::shared_ptr<ms::Session> const& session, ms::SurfaceCreationParameters const& params)
{
    auto const id = msh::ShellWrapper::create_surface(session, params);

    auto const surface = session->surface(id);
    auto const session_ready_observer = std::make_shared<SessionReadyObserver>(
        session_switcher, surface, session.get());

    surface->add_observer(session_ready_observer);

    return id;
}
