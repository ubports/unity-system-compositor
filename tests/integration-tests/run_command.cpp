/*
 * Copyright © 2015 Canonical Ltd.
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

#include "run_command.h"

#include <stdexcept>
#include <boost/throw_exception.hpp>

#include <cstdio>

std::string usc::test::run_command(std::string const& cmd)
{
    auto fp = ::popen(cmd.c_str(), "r");
    if (!fp)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to execute command: " + cmd));

    std::string output;

    char buffer[64];
    while (!std::feof(fp))
    {
        auto nread = std::fread(buffer, 1, 64, fp);
        output.append(buffer, nread);
    }

    ::pclose(fp);

    return output;
}
