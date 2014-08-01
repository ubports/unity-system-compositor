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

#include "session_coordinator.h"
#include "session_switcher.h"

#include <mir/scene/session.h>
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
        focus_controller.set_focus_to(scene_session);
    }

    bool corresponds_to(mir::frontend::Session const* s) override
    {
        return scene_session.get() == s;
    }

    std::shared_ptr<ms::Session> const scene_session;
    msh::FocusController& focus_controller;
};

}

usc::SessionCoordinator::SessionCoordinator(
    std::shared_ptr<ms::SessionCoordinator> const& wrapped,
    std::shared_ptr<SessionSwitcher> const& session_switcher)
    : msh::SessionCoordinatorWrapper{wrapped},
      session_switcher{session_switcher}
{
}

std::shared_ptr<mf::Session>
usc::SessionCoordinator::open_session(
    pid_t client_pid,
    std::string const& name,
    std::shared_ptr<mf::EventSink> const& sink)
{
    std::cerr << "Opening session " << name << std::endl;

    // We need ms::Session objects because that is what the focus controller
    // works with.  But the mf::SessionCoordinator interface deals with mf::Session objects.
    // So we cast here since in practice, these objects are also ms::Sessions.
    auto orig = std::dynamic_pointer_cast<ms::Session>(
        msh::SessionCoordinatorWrapper::open_session(client_pid, name, sink));
    if (!orig)
    {
        std::cerr << "Unexpected non-shell session" << std::endl;
        return std::shared_ptr<mf::Session>();
    }

    auto const usc_session = std::make_shared<UscSession>(orig, *this);

    session_switcher->add(usc_session, client_pid);

    return orig;
}

void usc::SessionCoordinator::close_session(
    std::shared_ptr<mf::Session> const& session)
{
    std::cerr << "Closing session " << session->name() << std::endl;

    msh::SessionCoordinatorWrapper::close_session(session);

    session_switcher->remove(session);
}
