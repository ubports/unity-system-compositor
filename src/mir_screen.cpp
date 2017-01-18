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

bool is_external(mir::graphics::DisplayConfigurationOutputType type)
{
    using mir::graphics::DisplayConfigurationOutputType;

    return type == DisplayConfigurationOutputType::vga ||
           type == DisplayConfigurationOutputType::dvii ||
           type == DisplayConfigurationOutputType::dvid ||
           type == DisplayConfigurationOutputType::dvia ||
           type == DisplayConfigurationOutputType::composite ||
           type == DisplayConfigurationOutputType::svideo ||
           type == DisplayConfigurationOutputType::component ||
           type == DisplayConfigurationOutputType::ninepindin ||
           type == DisplayConfigurationOutputType::displayport ||
           type == DisplayConfigurationOutputType::hdmia ||
           type == DisplayConfigurationOutputType::hdmib ||
           type == DisplayConfigurationOutputType::tv;
}

usc::ActiveOutputs count_active_outputs(
    mir::graphics::DisplayConfiguration const& display_configuration)
{
    usc::ActiveOutputs active_outputs{};

    display_configuration.for_each_output(
        [&active_outputs](mir::graphics::DisplayConfigurationOutput const& output)
        {
            if (output.connected && output.used)
            {
                if (is_external(output.type))
                    ++active_outputs.external;
                else
                    ++active_outputs.internal;
            }
        });

    return active_outputs;
}

}

usc::MirScreen::MirScreen(
    std::shared_ptr<mir::compositor::Compositor> const& compositor,
    std::shared_ptr<mir::graphics::Display> const& display)
    : compositor{compositor},
      display{display},
      current_power_mode{MirPowerMode::mir_power_mode_on},
      active_outputs_handler{[](ActiveOutputs const&){}}
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

void usc::MirScreen::register_active_outputs_handler(
    ActiveOutputsHandler const& handler)
{
    // It's not ideal to call the handler under lock, but we need this to
    // guarantee that after this function returns no invocation of the old
    // handler will be in progress. Alternatively, we would need to implement
    // an event loop.
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs_handler = handler;
    active_outputs_handler(active_outputs);
}

void usc::MirScreen::initial_configuration(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const& display_configuration)
{
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs = count_active_outputs(*display_configuration);
    active_outputs_handler(active_outputs);
}

void usc::MirScreen::configuration_applied(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const& display_configuration)
{
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs = count_active_outputs(*display_configuration);
    active_outputs_handler(active_outputs);
}

void usc::MirScreen::base_configuration_updated(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const&)
{
}

void usc::MirScreen::session_configuration_applied(
    std::shared_ptr<mir::frontend::Session> const&,
    std::shared_ptr<mir::graphics::DisplayConfiguration> const&)
{
}

void usc::MirScreen::session_configuration_removed(
    std::shared_ptr<mir::frontend::Session> const&)
{
}

void usc::MirScreen::configuration_failed(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const&,
    std::exception const&)
{
}

void usc::MirScreen::catastrophic_configuration_error(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const&,
    std::exception const&)
{
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

    display->configure(*displayConfig.get());

    if (mode == MirPowerMode::mir_power_mode_on)
        compositor->start();

    current_power_mode = mode;
}
catch (std::exception const&)
{
    log_exception_in(__func__);
}
