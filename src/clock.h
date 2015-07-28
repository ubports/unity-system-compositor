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

#ifndef USC_CLOCK_H_
#define USC_CLOCK_H_

#include <mir/time/types.h>
#include <chrono>

namespace usc
{

class Clock
{
public:
    virtual ~Clock() = default;

    virtual mir::time::Timestamp now() const = 0;

protected:
    Clock() = default;
    Clock(Clock const&) = delete;
    Clock& operator=(Clock const&) = delete;
};

}

#endif
