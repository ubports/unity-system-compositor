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

#ifndef USC_SHELL_H_
#define USC_SHELL_H_

#include <mir/shell/abstract_shell.h>

#include <memory>

namespace mir
{
namespace scene { class SurfaceConfigurator; }
namespace shell { class DisplayLayout; }
}

namespace usc
{
class SessionSwitcher;

class Shell : public mir::shell::AbstractShell
{
public:
    Shell(
        std::shared_ptr<mir::shell::InputTargeter> const& input_targeter,
        std::shared_ptr<mir::scene::SurfaceCoordinator> const& surface_coordinator,
        std::shared_ptr<mir::scene::SessionCoordinator> const& session_coordinator,
        std::shared_ptr<mir::scene::PromptSessionManager> const& prompt_session_manager,
        std::shared_ptr<mir::scene::SurfaceConfigurator> const& surface_configurator,
        std::shared_ptr<mir::shell::DisplayLayout> const& display_layout,
        std::shared_ptr<SessionSwitcher> const& session_switcher);

private:
    std::shared_ptr<mir::scene::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<mir::frontend::EventSink> const& sink) override;
    void close_session(std::shared_ptr<mir::scene::Session> const& session) override;

    mir::frontend::SurfaceId create_surface(
        std::shared_ptr<mir::scene::Session> const& session,
        mir::scene::SurfaceCreationParameters const& params) override;

    std::shared_ptr<SessionSwitcher> const session_switcher;
};

}

#endif
