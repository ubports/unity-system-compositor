/*
 * Copyright © 2014-2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "display_configuration_policy.h"

#include <mir/graphics/default_display_configuration_policy.h>
#include <mir/graphics/display_configuration.h>
#include <mir/server.h>
#include <mir/options/option.h>

namespace mg = mir::graphics;

void add_display_configuration_options_to(mir::Server& server)
{
    static char const* const display_config_opt = "display-config";
    static char const* const display_config_descr = "Display configuration [{clone,sidebyside,single}]";

    static char const* const sidebyside_opt_val = "sidebyside";
    static char const* const single_opt_val = "single";

    // Add choice of monitor configuration
    server.add_configuration_option(
        display_config_opt, display_config_descr, sidebyside_opt_val);

    server.wrap_display_configuration_policy(
        [&](std::shared_ptr<mg::DisplayConfigurationPolicy> const& wrapped)
        -> std::shared_ptr<mg::DisplayConfigurationPolicy>
        {
            auto const options = server.get_options();
            auto display_layout = options->get<std::string>(display_config_opt);

            auto layout_selector = wrapped;

            if (display_layout == sidebyside_opt_val)
                layout_selector = std::make_shared<mg::SideBySideDisplayConfigurationPolicy>();
            else if (display_layout == single_opt_val)
                layout_selector = std::make_shared<mg::SingleDisplayConfigurationPolicy>();

            return layout_selector;
        });
}
