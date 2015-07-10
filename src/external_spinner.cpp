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

#include "external_spinner.h"

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

namespace
{

void wait_for_child(int)
{
    while (waitpid(-1, nullptr, WNOHANG) > 0)
        continue;
}

}

usc::ExternalSpinner::ExternalSpinner(
    std::string const& executable,
    std::string const& mir_socket)
    : executable{executable},
      mir_socket{mir_socket},
      spinner_pid{0}
{
    struct sigaction sa;
    sigfillset(&sa.sa_mask);
    sa.sa_handler = wait_for_child;
    sa.sa_flags = 0;
    sigaction(SIGCHLD, &sa, nullptr);
}

usc::ExternalSpinner::~ExternalSpinner()
{
    kill();
}

void usc::ExternalSpinner::ensure_running()
{
    std::lock_guard<std::mutex> lock{mutex};

    if (executable.empty() || spinner_pid)
        return;

    auto pid = fork();
    if (!pid)
    {
        setenv("MIR_SOCKET", mir_socket.c_str(), 1);
        execlp(executable.c_str(), executable.c_str(), nullptr);
    }
    else
    {
        spinner_pid = pid;
    }
}

void usc::ExternalSpinner::kill()
{
    std::lock_guard<std::mutex> lock{mutex};

    if (spinner_pid)
    {
        ::kill(spinner_pid, SIGTERM);
        spinner_pid = 0;
    }
}

pid_t usc::ExternalSpinner::pid()
{
    std::lock_guard<std::mutex> lock{mutex};

    return spinner_pid;
}
