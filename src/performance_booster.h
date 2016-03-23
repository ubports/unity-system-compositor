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

#ifndef USC_PERFORMANCE_BOOSTER_H_
#define USC_PERFORMANCE_BOOSTER_H_

#include <memory>

namespace usc
{
class PerformanceBooster
{
public:
    PerformanceBooster() = default;
    PerformanceBooster(const PerformanceBooster&) = delete;
    virtual ~PerformanceBooster() = default;
    PerformanceBooster& operator=(const PerformanceBooster&) = delete;

    virtual void enable_performance_boost_during_user_interaction() = 0;
    virtual void disable_performance_boost_during_user_interaction() = 0;
};

std::shared_ptr<PerformanceBooster> platform_default_performance_booster();
}

#endif // USC_PERFORMANCE_BOOSTER_H_
