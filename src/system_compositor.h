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

#ifndef SYSTEM_COMPOSITOR_H_
#define SYSTEM_COMPOSITOR_H_

#include "dm_connection.h"

#include <mir/default_server_configuration.h>
#include <mir/shell/application_session.h>

class Configuration;

class SystemCompositor : public DMMessageHandler
{
public:
    void run(int argc, char const** argv);
    void pause();
    void resume();

private:
    std::shared_ptr<mir::DefaultServerConfiguration> config;
    boost::asio::io_service io_service;
    std::shared_ptr<DMConnection> dm_connection;
    std::shared_ptr<mir::shell::ApplicationSession> active_session;

    void set_active_session(std::string client_name);
    void set_next_session(std::string client_name);
    void main();
};

#endif /* SYSTEM_COMPOSITOR_H_ */
