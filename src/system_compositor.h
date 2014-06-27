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
#include <QProcess>

namespace mir { namespace scene { class Session; } }

class SystemCompositorShell;
class SystemCompositorServerConfiguration;
class ScreenStateHandler;
class PowerKeyHandler;

class SystemCompositor : public DMMessageHandler
{
public:
    void run(int argc, char **argv);
    void pause();
    void resume();
    pid_t get_spinner_pid() const;
    void ensure_spinner();
    void kill_spinner();

private:
    std::shared_ptr<SystemCompositorServerConfiguration> config;
    std::shared_ptr<SystemCompositorShell> shell;
    boost::asio::io_service io_service;
    std::shared_ptr<DMConnection> dm_connection;
    std::shared_ptr<ScreenStateHandler> screen_state_handler;
    std::shared_ptr<PowerKeyHandler> power_key_handler;
    QProcess spinner_process;

    void set_active_session(std::string client_name);
    void set_next_session(std::string client_name);
    void main();
    void qt_main(int argc, char **argv);
};

#endif /* SYSTEM_COMPOSITOR_H_ */
