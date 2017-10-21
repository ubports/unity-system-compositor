/*
 * Copyright © 2017 Joseph Rushton Wakeling
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
 * Authored by: Joseph Rushton Wakeling <joe@webdrake.net>
 */

#ifndef USC_CREATE_DM_CONNECTION_H
#define USC_CREATE_DM_CONNECTION_H

#include <mir/optional_value.h>
#include <memory>

namespace usc
{
    class DMConnection;
    class DMMessageHandler;

    std::shared_ptr<DMConnection> create_dm_connection(
        mir::optional_value<int> from_dm_fd,
        mir::optional_value<int> to_dm_fd,
        std::string debug_active_session_name,
        bool debug_without_dm,
        std::shared_ptr<DMMessageHandler> const& dm_message_handler);
}

#endif // USC_CREATE_DM_CONNECTION_H

