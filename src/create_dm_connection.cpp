/*
 * Copyright © 2013-2014 Canonical Ltd., © 2017 Joseph Rushton Wakeling
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
 *              Joseph Rushton Wakeling <joe@webdrake.net>
 */

#include "asio_dm_connection.h"
#include "create_dm_connection.h"

#include <mir/abnormal_exit.h>

#include <boost/exception/all.hpp>

namespace
{

struct NullDMMessageHandler : usc::DMConnection
{
    explicit NullDMMessageHandler(
        std::shared_ptr<usc::DMMessageHandler> const& dm_message_handler,
        std::string const& client_name) :
            dm_message_handler{dm_message_handler},
            client_name{client_name}
    {}

    ~NullDMMessageHandler() = default;

    void start() override
    {
        dm_message_handler->set_active_session(client_name);
    };

    std::shared_ptr<usc::DMMessageHandler> const dm_message_handler;
    std::string const client_name;
};

}

namespace usc
{

std::shared_ptr<DMConnection> create_dm_connection(
    mir::optional_value<int> from_dm_fd,
    mir::optional_value<int> to_dm_fd,
    std::string debug_active_session_name,
    bool debug_without_dm,
    std::shared_ptr<DMMessageHandler> const& dm_message_handler)
{
    if (from_dm_fd.is_set() && to_dm_fd.is_set())
    {
        return std::make_shared<AsioDMConnection>(
            from_dm_fd.value(),
            to_dm_fd.value(),
            dm_message_handler);
    }
    else if (debug_without_dm)
    {
        return std::make_shared<NullDMMessageHandler>(
            dm_message_handler,
            debug_active_session_name);
    }
    else
    {
        BOOST_THROW_EXCEPTION(mir::AbnormalExit("to and from FDs are required for display manager"));
    }
}

}
