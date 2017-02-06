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

#ifndef USC_MIR_SCREEN_H_
#define USC_MIR_SCREEN_H_

#include <mir/graphics/display_configuration_observer.h>
#include "screen.h"

#include <chrono>
#include <memory>
#include <mutex>

namespace mir
{
namespace compositor { class Compositor; }
namespace graphics {class Display; struct UserDisplayConfigurationOutput;}
}

namespace usc
{

class MirScreen: public Screen, public mir::graphics::DisplayConfigurationObserver
{
public:
    MirScreen(std::shared_ptr<mir::compositor::Compositor> const& compositor,
              std::shared_ptr<mir::graphics::Display> const& display);
    ~MirScreen();

    // From Screen
    void turn_on(OutputFilter output_filter) override;
    void turn_off(OutputFilter output_filter) override;
    void register_active_outputs_handler(ActiveOutputsHandler const& handler) override;

    // From DisplayConfigurationObserver
    void initial_configuration(
        std::shared_ptr<mir::graphics::DisplayConfiguration const> const& display_configuration) override;
    void configuration_applied(
        std::shared_ptr<mir::graphics::DisplayConfiguration const> const& display_configuration) override;

    void base_configuration_updated(
        std::shared_ptr<mir::graphics::DisplayConfiguration const> const&) override;
    void session_configuration_applied(
        std::shared_ptr<mir::frontend::Session> const&,
        std::shared_ptr<mir::graphics::DisplayConfiguration> const&) override;
    void session_configuration_removed(
        std::shared_ptr<mir::frontend::Session> const&) override;
    void configuration_failed(
        std::shared_ptr<mir::graphics::DisplayConfiguration const> const&,
        std::exception const&) override;
    void catastrophic_configuration_error(
        std::shared_ptr<mir::graphics::DisplayConfiguration const> const&,
        std::exception const&) override;

private:
    using SetPowerModeFilter = bool(*)(mir::graphics::UserDisplayConfigurationOutput const&);
    void set_power_mode(MirPowerMode mode, SetPowerModeFilter const& filter);

    std::shared_ptr<mir::compositor::Compositor> const compositor;
    std::shared_ptr<mir::graphics::Display> const display;

    std::mutex active_outputs_mutex;
    ActiveOutputsHandler active_outputs_handler;
    ActiveOutputs active_outputs;
};

}

#endif
