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

#include "extract_socket_config.h"

#include <mir/options/option.h>
#include <mir/server.h>

usc::ExtractSocketConfig::ExtractSocketConfig(Callback const& callback)
    : callback{callback}
{
}

usc::ExtractSocketConfig::~ExtractSocketConfig() = default;

void usc::ExtractSocketConfig::operator()(mir::Server& server) const
{
    server.add_pre_init_callback([&]
        {
            callback(
                server.get_options()->get("file", "/tmp/mir-socket"),
                server.get_options()->is_set("no-file"));
        });
}
