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

usc::ExternalSpinner::ExternalSpinner(
    std::string const& executable,
    std::string const& mir_socket)
    : executable{executable},
      mir_socket{mir_socket}
{
}

void usc::ExternalSpinner::ensure_running()
{
    if (executable.empty() || process.state() != QProcess::NotRunning)
        return;

    // Launch spinner process to provide default background when a session isn't ready
    QStringList env = QProcess::systemEnvironment();
    env << "MIR_SOCKET=" + QString::fromStdString(mir_socket);
    process.setEnvironment(env);
    process.start(executable.c_str());
}

void usc::ExternalSpinner::kill()
{
    process.close();
}

pid_t usc::ExternalSpinner::pid()
{
    return process.processId();
}
