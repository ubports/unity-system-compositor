/*
 * Copyright © 2014-2015 Canonical Ltd.
 * Copyright (C) 2020 UBports foundation.
 * Author(s): Ratchanan Srirattanamet <ratchanan@ubports.com>
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
            if (output.connected &&
                output.used &&
                output.power_mode == MirPowerMode::mir_power_mode_on)
            {
                if (is_external(output.type))
                    ++active_outputs.external;
                else
                    ++active_outputs.internal;
            }
        });

    return active_outputs;
}

bool has_active_outputs(
    mir::graphics::DisplayConfiguration const& display_configuration)
{
    auto const active_outputs = count_active_outputs(display_configuration);
    return active_outputs.internal + active_outputs.external > 0;
}

bool all_outputs_filter(mir::graphics::UserDisplayConfigurationOutput const&)
{
    return true;
}

bool internal_outputs_filter(mir::graphics::UserDisplayConfigurationOutput const& output)
{
    return !is_external(output.type);
}

bool external_outputs_filter(mir::graphics::UserDisplayConfigurationOutput const& output)
{
    return is_external(output.type);
}

auto get_power_mode_filter_for_output_filter(usc::OutputFilter output_filter)
{
    switch (output_filter)
    {
        case usc::OutputFilter::all: return all_outputs_filter;
        case usc::OutputFilter::internal: return internal_outputs_filter;
        case usc::OutputFilter::external: return external_outputs_filter;
        default: return all_outputs_filter;
    }

    return all_outputs_filter;
}

}

usc::MirScreen::MirScreen(
    std::shared_ptr<mir::compositor::Compositor> const& compositor,
    std::shared_ptr<mir::graphics::Display> const& display)
    : compositor{compositor},
      display{display}
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

void usc::MirScreen::turn_on(OutputFilter output_filter)
{
    auto const filter_func = get_power_mode_filter_for_output_filter(output_filter);
    set_power_mode(MirPowerMode::mir_power_mode_on, filter_func);
}

void usc::MirScreen::turn_off(OutputFilter output_filter)
{
    auto const filter_func = get_power_mode_filter_for_output_filter(output_filter);
    set_power_mode(MirPowerMode::mir_power_mode_off, filter_func);
}

void usc::MirScreen::register_active_outputs_handler(
    void * ownerKey, ActiveOutputsHandler const& handler)
{
    // It's not ideal to call the handler under lock, but we need this to
    // guarantee that after this function returns no invocation of the old
    // handler will be in progress. Alternatively, we would need to implement
    // an event loop.
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs_handlers[ownerKey] = handler;
    // Call only the new handler immediately.
    handler(active_outputs);
}

void usc::MirScreen::unregister_active_outputs_handler(
    void * ownerKey)
{
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs_handlers.erase(ownerKey);
}

void usc::MirScreen::initial_configuration(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const& display_configuration)
{
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs = count_active_outputs(*display_configuration);
    for (auto const& pair: active_outputs_handlers)
        pair.second(active_outputs);
}

void usc::MirScreen::configuration_applied(
    std::shared_ptr<mir::graphics::DisplayConfiguration const> const& display_configuration)
{
    std::lock_guard<std::mutex> lock{active_outputs_mutex};
    active_outputs = count_active_outputs(*display_configuration);
    for (auto const& pair: active_outputs_handlers)
        pair.second(active_outputs);
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

void usc::MirScreen::set_power_mode(MirPowerMode mode, SetPowerModeFilter const& filter)
try
{
    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();

    displayConfig->for_each_output(
        [&](const mg::UserDisplayConfigurationOutput displayConfigOutput) {
            if (displayConfigOutput.connected &&
                displayConfigOutput.used &&
                filter(displayConfigOutput))
            {
                displayConfigOutput.power_mode = mode;
            }
        }
    );

    compositor->stop();

    display->configure(*displayConfig.get());

    if (has_active_outputs(*displayConfig))
        compositor->start();

    // Setting power mode is considered a configuration change.
    configuration_applied(displayConfig);
}
catch (std::exception const&)
{
    log_exception_in(__func__);
}
