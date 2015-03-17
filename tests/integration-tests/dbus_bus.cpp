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

#include "dbus_bus.h"

#include <sstream>
#include <stdexcept>

#include <cstdio>
#include <cstdlib>
#include <csignal>

namespace
{

std::string run_command(std::string const& cmd)
{
    auto fp = ::popen(cmd.c_str(), "r");
    if (!fp)
        throw std::runtime_error("Failed to execute command: " + cmd);

    std::string reply;

    char buffer[64];
    while (!feof(fp))
    {
        auto nread = fread(buffer, 1, 64, fp);
        reply.append(buffer, nread);
    }

    pclose(fp);

    return reply;
}

}

usc::test::DBusBus::DBusBus()
    : pid{0}
{
    auto launch = run_command("dbus-daemon --session --print-address=1 --print-pid=1 --fork");
    std::stringstream ss{launch};

    std::getline(ss, address_);
    ss >> pid;

    if (address_.empty())
        throw std::runtime_error("Failed to get dbus bus address");

    if (pid == 0)
        throw std::runtime_error("Failed to get dbus bus pid");
}

usc::test::DBusBus::~DBusBus()
{
    kill(pid, SIGTERM);
}

std::string usc::test::DBusBus::address()
{
    return address_;
}
