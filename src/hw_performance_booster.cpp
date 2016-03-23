/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include "hw_performance_booster.h"

#include <boost/exception/all.hpp>

#include <stdexcept>

namespace
{
UHardwareBooster* create_hw_booster_or_throw()
{
    if (auto result = u_hardware_booster_new())
        return result;

    BOOST_THROW_EXCEPTION(std::runtime_error{"Failed to acquire a valid UHardwareBooster instance."});
}
}

usc::HwPerformanceBooster::HwPerformanceBooster() : hw_booster{create_hw_booster_or_throw(), [](UHardwareBooster* booster) { if (booster) u_hardware_booster_unref(booster); }}
{
}

void usc::HwPerformanceBooster::enable_performance_boost_during_user_interaction()
{
    u_hardware_booster_enable_scenario(hw_booster.get(), U_HARDWARE_BOOSTER_SCENARIO_USER_INTERACTION);
}

void usc::HwPerformanceBooster::disable_performance_boost_during_user_interaction()
{
    u_hardware_booster_disable_scenario(hw_booster.get(), U_HARDWARE_BOOSTER_SCENARIO_USER_INTERACTION);
}
