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

#include "src/external_spinner.h"
#include "run_command.h"
#include "spin_wait.h"

#include <fstream>
#include <chrono>
#include <thread>
#include <stdexcept>

#include <boost/throw_exception.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace
{

std::string executable_path()
{
    std::vector<char> link_vec(64, 0);

    ssize_t nread{0};
    
    do
    {
        link_vec.resize(link_vec.size() * 2);
        nread = readlink("/proc/self/exe", link_vec.data(), link_vec.size());
    }
    while (nread == static_cast<ssize_t>(link_vec.size()));

    std::string link{link_vec.begin(), link_vec.begin() + nread};

    return link.substr(0, link.rfind("/"));
}

std::vector<pid_t> pidof(std::string const& process_name)
{
    auto cmd_string = "pidof " + process_name;
    auto const pid_str = usc::test::run_command(cmd_string);

    std::vector<pid_t> pids;

    std::stringstream ss{pid_str};

    while (ss)
    {
        pid_t pid{0};
        ss >> pid;
        if (pid > 0)
            pids.push_back(pid);
    }

    return pids;
}

struct AnExternalSpinner : testing::Test
{
    std::vector<pid_t> spinner_pids()
    {
        std::vector<pid_t> pids;

        usc::test::spin_wait_for_condition_or_timeout(
            [&pids, this] { pids = pidof(spinner_cmd); return !pids.empty(); },
            timeout);

        if (pids.empty())
            BOOST_THROW_EXCEPTION(std::runtime_error("spinner_pids timed out"));

        return pids;
    }

    std::vector<std::string> environment_of_spinner()
    {
        auto const pids = spinner_pids();
        if (pids.size() > 1)
            BOOST_THROW_EXCEPTION(std::runtime_error("Detected multiple spinner processes"));
        std::vector<std::string> env;

        std::string const proc_path{"/proc/" + std::to_string(pids[0]) + "/environ"};
        std::ifstream env_file{proc_path};
        std::string val;

        while (std::getline(env_file, val, '\0'))
            env.push_back(val);

        return env;
    }

    void wait_for_spinner_to_terminate()
    {
        usc::test::spin_wait_for_condition_or_timeout(
            [this] { return pidof(spinner_cmd).empty(); },
            timeout);
    }

    std::string const spinner_cmd{executable_path() + "/usc_test_helper_wait_for_signal"};
    std::string const mir_socket{"usc_mir_socket"};
    std::chrono::milliseconds const timeout{3000};
    usc::ExternalSpinner spinner{spinner_cmd, mir_socket};
};

}

TEST_F(AnExternalSpinner, starts_spinner_process)
{
    using namespace testing;

    spinner.ensure_running();

    EXPECT_THAT(spinner_pids(), SizeIs(1));
}

TEST_F(AnExternalSpinner, kills_spinner_process_on_destruction)
{
    using namespace testing;

    {
        usc::ExternalSpinner another_spinner{spinner_cmd, "bla"};
        another_spinner.ensure_running();
        EXPECT_THAT(spinner_pids(), SizeIs(1));
    }

    wait_for_spinner_to_terminate();
}

TEST_F(AnExternalSpinner, kills_spinner_process_on_request)
{
    using namespace testing;

    spinner.ensure_running();
    EXPECT_THAT(spinner_pids(), SizeIs(1));
    spinner.kill();

    wait_for_spinner_to_terminate();
}

TEST_F(AnExternalSpinner, starts_spinner_process_only_once)
{
    using namespace testing;

    spinner.ensure_running();
    spinner.ensure_running();

    EXPECT_THAT(spinner_pids(), SizeIs(1));
}

TEST_F(AnExternalSpinner, sets_mir_socket_in_spinner_process_environment)
{
    using namespace testing;

    spinner.ensure_running();

    EXPECT_THAT(environment_of_spinner(), Contains("MIR_SOCKET=" + mir_socket));
}
