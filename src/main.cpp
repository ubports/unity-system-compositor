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

#include "system_compositor.h"
#include "server.h"

#include <mir/report_exception.h>
#include <iostream>

namespace usc { class SessionSwitcher; }

int main(int argc, char *argv[])
try
{
    std::shared_ptr<usc::SessionSwitcher> session_switcher;
    auto const config = std::make_shared<usc::Server>(argc, argv, session_switcher);

    if (config->get_options()->is_set("version"))
    {
        std::cerr << "unity-system-compositor " << USC_VERSION << std::endl;
        return 0;
    }

    auto the_session_switcher = [&session_switcher]() { return session_switcher; };

    usc::SystemCompositor system_compositor{the_session_switcher};
    system_compositor(*config);

    config->run();

    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
