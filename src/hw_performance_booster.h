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

#ifndef USC_HW_PERFORMANCE_BOOSTER_H_
#define USC_HW_PERFORMANCE_BOOSTER_H_

#include "performance_booster.h"

#include <ubuntu/hardware/booster.h>

namespace usc
{
class HwPerformanceBooster : public PerformanceBooster
{
public:
    HwPerformanceBooster();
    ~HwPerformanceBooster();

    void enable_performance_boost_during_user_interaction() override;
    void disable_performance_boost_during_user_interaction() override;

protected:
    UHardwareBooster* hw_booster;
};
}

#endif // USC_HW_PERFORMANCE_BOOSTER_H_
