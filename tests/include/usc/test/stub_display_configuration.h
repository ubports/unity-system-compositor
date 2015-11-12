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
        conf_output.power_mode = MirPowerMode::mir_power_mode_on;
    }

    StubDisplayConfiguration(mir::graphics::DisplayConfigurationOutput const& output)
        : conf_output(output)
    {
    }

    void for_each_card(std::function<void(mir::graphics::DisplayConfigurationCard const&)>) const override
    {
    }

    void for_each_output(std::function<void(mir::graphics::DisplayConfigurationOutput const&)> f) const override
    {
        f(conf_output);
    }

    void for_each_output(std::function<void(mir::graphics::UserDisplayConfigurationOutput&)> f)
    {
        mir::graphics::UserDisplayConfigurationOutput user{conf_output};
        f(user);
    }

    std::unique_ptr<mir::graphics::DisplayConfiguration> clone() const override
    {
        return std::make_unique<StubDisplayConfiguration>(conf_output);
    }

    mir::graphics::DisplayConfigurationOutput conf_output;
};

}
}

#endif
