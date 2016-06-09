/*
 * Copyright © 2016 Canonical Ltd.
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

#ifndef USC_POWER_BUTTON_EVENT_SINK_H_
#define USC_POWER_BUTTON_EVENT_SINK_H_

namespace usc
{

class PowerButtonEventSink
{
public:
    virtual ~PowerButtonEventSink() = default;

    virtual void notify_press() = 0;
    virtual void notify_release() = 0;

protected:
    PowerButtonEventSink() = default;
    PowerButtonEventSink(PowerButtonEventSink const&) = delete;
    PowerButtonEventSink& operator=(PowerButtonEventSink const&) = delete;
};

}

#endif
