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

#ifndef USC_STEADY_CLOCK_H_
#define USC_STEADY_CLOCK_H_

#include "clock.h"

namespace usc
{

class SteadyClock : public Clock
{
public:
    mir::time::Timestamp now() const override;

private:
    std::chrono::steady_clock clock;
};

}

#endif
