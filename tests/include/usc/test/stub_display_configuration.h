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
 */

#ifndef USC_TEST_STUB_DISPLAY_CONFIGURATION_H_
#define USC_TEST_STUB_DISPLAY_CONFIGURATION_H_

#include <mir/graphics/display_configuration.h>

namespace usc
{
namespace test
{

struct StubDisplayConfiguration : mir::graphics::DisplayConfiguration
{
    StubDisplayConfiguration()
    {
        internal_active_conf_output.power_mode = MirPowerMode::mir_power_mode_on;
        internal_active_conf_output.type = mir::graphics::DisplayConfigurationOutputType::lvds;
        internal_active_conf_output.used = true;
        internal_active_conf_output.connected = true;

        external_active_conf_output.power_mode = MirPowerMode::mir_power_mode_on;
        external_active_conf_output.type = mir::graphics::DisplayConfigurationOutputType::dvid;
        external_active_conf_output.used = true;
        external_active_conf_output.connected = true;

        inactive_conf_output.power_mode = MirPowerMode::mir_power_mode_off;
        inactive_conf_output.used = false;
        inactive_conf_output.connected = false;
    }

    StubDisplayConfiguration(
        int num_internal_active_outputs,
        int num_external_active_outputs,
        int num_inactive_outputs)
        : StubDisplayConfiguration{}
    {
          this->num_internal_active_outputs = num_internal_active_outputs;
          this->num_external_active_outputs = num_external_active_outputs;
          this->num_inactive_outputs = num_inactive_outputs;
    }

    StubDisplayConfiguration(mir::graphics::DisplayConfigurationOutput const& output)
        : StubDisplayConfiguration{}
    {
          internal_active_conf_output = output;
    }

    void for_each_card(std::function<void(mir::graphics::DisplayConfigurationCard const&)>) const override
    {
    }

    void for_each_output(std::function<void(mir::graphics::DisplayConfigurationOutput const&)> f) const override
    {
        for (int i = 0; i < num_internal_active_outputs; ++i)
            f(internal_active_conf_output);
        for (int i = 0; i < num_external_active_outputs; ++i)
            f(external_active_conf_output);
        for (int i = 0; i < num_inactive_outputs; ++i)
            f(inactive_conf_output);
    }

    void for_each_output(std::function<void(mir::graphics::UserDisplayConfigurationOutput&)> f)
    {
        for (int i = 0; i < num_internal_active_outputs; ++i)
        {
            mir::graphics::UserDisplayConfigurationOutput user{internal_active_conf_output};
            f(user);
        }
        for (int i = 0; i < num_external_active_outputs; ++i)
        {
            mir::graphics::UserDisplayConfigurationOutput user{external_active_conf_output};
            f(user);
        }
        for (int i = 0; i < num_inactive_outputs; ++i)
        {
            mir::graphics::UserDisplayConfigurationOutput user{inactive_conf_output};
            f(user);
        }
    }

    std::unique_ptr<mir::graphics::DisplayConfiguration> clone() const override
    {
        return std::make_unique<StubDisplayConfiguration>(internal_active_conf_output);
    }

    int num_internal_active_outputs{1};
    int num_external_active_outputs{0};
    int num_inactive_outputs{0};

    mir::graphics::DisplayConfigurationOutput internal_active_conf_output;
    mir::graphics::DisplayConfigurationOutput external_active_conf_output;
    mir::graphics::DisplayConfigurationOutput inactive_conf_output;
};

}
}

#endif
