/*
 * Copyright © 2014-2015 Canonical Ltd.
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
 */

#include "mir_screen.h"

#include <mir/compositor/compositor.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>
#include <mir/log.h>
#include <mir/report_exception.h>

#include <cstdio>
#include <sstream>

#include <assert.h>

#include "performance_booster.h"
#include "server.h"

namespace mi = mir::input;
namespace mg = mir::graphics;

namespace
{
void log_exception_in(char const* const func)
{
    static char const* const warning_format = "%s failed: %s";
    std::stringstream buffer;

    mir::report_exception(buffer);
    mir::log(::mir::logging::Severity::warning, "usc::MirScreen", warning_format, func, buffer.str().c_str());
}
}

usc::MirScreen::MirScreen(
    std::shared_ptr<usc::PerformanceBooster> const& perf_booster,
    std::shared_ptr<mir::compositor::Compositor> const& compositor,
    std::shared_ptr<mir::graphics::Display> const& display)
    : current_power_mode{MirPowerMode::mir_power_mode_on}
{
    try
    {
        /*
         * Make sure the compositor is running as certain conditions can
         * cause Mir to tear down the compositor threads before we get
         * to this point. See bug #1410381.
         */
        compositor->start();
    }
    catch (...)
    {
        log_exception_in(__func__);
        throw;
    }
}

usc::MirScreen::~MirScreen() = default;

void usc::MirScreen::turn_on()
{
    set_power_mode(MirPowerMode::mir_power_mode_on);
}

void usc::MirScreen::turn_off()
{
    set_power_mode(MirPowerMode::mir_power_mode_off);
}

void usc::MirScreen::set_power_mode(MirPowerMode mode)
try
{
    if (current_power_mode == mode)
        return;

    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();

    displayConfig->for_each_output(
        [&](const mg::UserDisplayConfigurationOutput displayConfigOutput) {
            displayConfigOutput.power_mode = mode;
        }
    );

    compositor->stop();

    bool const power_on = mode == MirPowerMode::mir_power_mode_on;
    if (power_on)
    {
        perf_booster->enable_performance_boost_during_user_interaction();
    }
    else
    {
        perf_booster->disable_performance_boost_during_user_interaction();
    }

    display->configure(*displayConfig.get());

    if (power_on)
        compositor->start();

    current_power_mode = mode;
}
catch (std::exception const&)
{
    log_exception_in(__func__);
}
