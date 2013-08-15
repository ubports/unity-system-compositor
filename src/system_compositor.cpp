/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Robert Ancell <robert.ancell@canonical.com>
 */

#include "system_compositor.h"

#include <mir/run_mir.h>
#include <mir/shell/session.h>
#include <mir/shell/session_container.h>
#include <mir/shell/surface_builder.h>
#include <mir/shell/surface_creation_parameters.h>
#include <mir/shell/focus_setter.h>
#include <mir/surfaces/depth_id.h>
#include <mir/surfaces/surface_stack_model.h>
#include <mir/input/cursor_listener.h>

#include <iostream>
#include <thread>

namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace mi = mir::input;

class SystemCompositorSurfaceBuilder : public msh::SurfaceBuilder
{
public:
    SystemCompositorSurfaceBuilder(std::shared_ptr<ms::SurfaceStackModel> const& surface_stack)
        : surface_stack(surface_stack)
    {
    }

    std::weak_ptr<ms::Surface> create_surface(msh::Session *session, msh::SurfaceCreationParameters const& params)
    {
        static ms::DepthId const default_surface_depth{0};
        static ms::DepthId const greeter_surface_depth{1};

        auto depth_params = params;
        if (session->name().find("greeter-") == 0)
        {
            depth_params.depth = greeter_surface_depth;
        }
        else
        {
            depth_params.depth = default_surface_depth;
        }
        return surface_stack->create_surface(depth_params);
    }

    void destroy_surface(std::weak_ptr<ms::Surface> const& surface)
    {
        surface_stack->destroy_surface(surface);
    }

private:
    std::shared_ptr<ms::SurfaceStackModel> const surface_stack;
};

class SystemCompositorServerConfiguration : public mir::DefaultServerConfiguration
{
public:
    SystemCompositorServerConfiguration(int argc, char const** argv)
        : mir::DefaultServerConfiguration(argc, argv)
    {
        namespace po = boost::program_options;

        add_options()
            ("from-dm-fd", po::value<int>(),  "File descriptor of read end of pipe from display manager [int]")
            ("to-dm-fd", po::value<int>(),  "File descriptor of write end of pipe to display manager [int]");
        add_options()
            ("version", "Show version of Unity System Compositor");
    }

    int from_dm_fd()
    {
        return the_options()->get("from-dm-fd", -1);
    }

    int to_dm_fd()
    {
        return the_options()->get("to-dm-fd", -1);
    }

    bool show_version()
    {
        return the_options()->is_set ("version");
    }
    
    std::shared_ptr<mi::CursorListener> the_cursor_listener() override
    {
        struct NullCursorListener : public mi::CursorListener
        {
            void cursor_moved_to(float, float) override
            {
            }
        };
        return std::make_shared<NullCursorListener>();
    }

    std::shared_ptr<msh::SurfaceBuilder> the_surface_builder() override
    {
        return surface_builder(
            [this]()
            {
                return std::make_shared<SystemCompositorSurfaceBuilder>(the_surface_stack_model());
            });
    }

private:
    mir::CachedPtr<msh::SurfaceBuilder> surface_builder;
};

void SystemCompositor::run(int argc, char const** argv)
{
    auto c = std::make_shared<SystemCompositorServerConfiguration>(argc, argv);
    config = c;
  
    if (c->show_version())
    {
        std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
        return;
    }

    dm_connection = std::make_shared<DMConnection>(io_service, c->from_dm_fd(), c->to_dm_fd());

    struct ScopeGuard
    {
        explicit ScopeGuard(boost::asio::io_service& io_service) : io_service(io_service) {}
        ~ScopeGuard() { io_service.stop(); if (thread.joinable()) thread.join(); }

        boost::asio::io_service& io_service;
        std::thread thread;
    } guard(io_service);

    mir::run_mir(*config, [&](mir::DisplayServer&)
        {
            guard.thread = std::thread(&SystemCompositor::main, this);
        });
}

void SystemCompositor::set_active_session(std::string client_name)
{
    std::cerr << "set_active_session" << std::endl;

    std::shared_ptr<msh::Session> session;
    config->the_shell_session_container()->for_each([&client_name, &session](std::shared_ptr<msh::Session> const& s)
    {
        if (s->name() == client_name)
            session = s;
    });

    if (session)
        config->the_shell_focus_setter()->set_focus_to(session);
    else
        std::cerr << "Unable to set active session, unknown client name " << client_name << std::endl;
}

void SystemCompositor::set_next_session(std::string client_name)
{
    std::cerr << "set_next_session" << std::endl;

    std::shared_ptr<msh::Session> session;
    config->the_shell_session_container()->for_each([&client_name, &session](std::shared_ptr<msh::Session> const& s)
    {
        if (s->name() == client_name)
            session = s;
    });

    if (session)
        config->the_shell_focus_setter()->set_focus_to(session); // depth id will keep it separate from greeter
    else
        std::cerr << "Unable to set next session, unknown client name " << client_name << std::endl;
}

void SystemCompositor::main()
{
    dm_connection->set_handler(this);
    dm_connection->start();
    dm_connection->send_ready();

    io_service.run();
}
