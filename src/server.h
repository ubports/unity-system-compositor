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

#ifndef USC_SERVER_H_
#define USC_SERVER_H_

#include <mir/server.h>
#include <mir/cached_ptr.h>
#include <mir/options/option.h>

namespace usc
{
class SessionSwitcher;
class Screen;
class InputConfiguration;

class Server : private mir::Server
{
public:
    explicit Server(int argc, char** argv,
        std::shared_ptr<SessionSwitcher>& session_switcher);

    using mir::Server::add_init_callback;
    using mir::Server::get_options;
    using mir::Server::run;
    using mir::Server::the_main_loop;
    using mir::Server::the_composite_event_filter;
    using mir::Server::the_display;
    using mir::Server::the_compositor;
    using mir::Server::the_touch_visualizer;

    virtual std::shared_ptr<Screen> the_screen();
    virtual std::shared_ptr<InputConfiguration> the_input_configuration();

private:
    mir::CachedPtr<Screen> screen;
    mir::CachedPtr<InputConfiguration> input_configuration;
};

}

#endif
