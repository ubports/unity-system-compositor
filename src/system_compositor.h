/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 *              Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef USC_SYSTEM_COMPOSITOR_H_
#define USC_SYSTEM_COMPOSITOR_H_

#include <memory>

namespace usc
{

class Server;
class DMConnection;
class Spinner;
class ScreenEventHandler;
class MirScreen;

class SystemCompositor
{
public:
    explicit SystemCompositor(std::shared_ptr<Server> const& server);
    void run();

private:
    void qt_main();

    std::shared_ptr<Server> const server;
    std::shared_ptr<DMConnection> dm_connection;
    std::shared_ptr<Spinner> const spinner;
    std::shared_ptr<MirScreen> mir_screen;
    std::shared_ptr<ScreenEventHandler> screen_event_handler;
};

}

#endif /* USC_SYSTEM_COMPOSITOR_H_ */
