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

#define MIR_LOG_COMPONENT "UnitySystemCompositor"

#include "performance_booster.h"
#include "null_performance_booster.h"

#include <mir/log.h>

#include <boost/exception/all.hpp>

#if defined(USC_HAVE_UBUNTU_PLATFORM_HARDWARE_API)
#include "hw_performance_booster.h"

std::shared_ptr<usc::PerformanceBooster> usc::platform_default_performance_booster()
{
    // We are treating access to a functional implementation of PerformanceBooster as optional.
    // With that, we gracefully fall back to a NullImplementation if we cannot gain access
    // to hw-provided booster capabilities.
    try
    {
        return std::make_shared<HwPerformanceBooster>();
    }
    catch (boost::exception const& e)
    {
        mir::log_warning(boost::diagnostic_information(e));
    }

    return std::make_shared<NullPerformanceBooster>();
}
#else
std::shared_ptr<usc::PerformanceBooster> usc::platform_default_performance_booster()
{
    return std::make_shared<NullPerformanceBooster>();
}
#endif // USC_HAVE_UBUNTU_PLATFORM_HARDWARE_API
