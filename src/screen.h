/*
 * Copyright © 2014-2015 Canonical Ltd.
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

#ifndef USC_SCREEN_H_
#define USC_SCREEN_H_

#include <mir_toolkit/common.h>
#include <functional>

enum class PowerStateChangeReason;

namespace usc
{

using PowerStateChangeHandler = std::function<void(MirPowerMode, PowerStateChangeReason)>;

class Screen
{
public:
    virtual ~Screen() = default;

    virtual void enable_inactivity_timers(bool enable) = 0;
    virtual void toggle_screen_power_mode(PowerStateChangeReason reason) = 0;
    virtual void keep_display_on_temporarily() = 0;

    virtual void set_screen_power_mode(MirPowerMode mode, PowerStateChangeReason reason) = 0;
    virtual void keep_display_on(bool on) = 0;
    virtual void set_brightness(int brightness) = 0;
    virtual void enable_auto_brightness(bool enable) = 0;
    virtual void set_inactivity_timeouts(int power_off_timeout, int dimmer_timeout) = 0;

    virtual void set_touch_visualization_enabled(bool enabled) = 0;
    virtual void register_power_state_change_handler(
            PowerStateChangeHandler const& handler) = 0;

protected:
    Screen() = default;
    Screen(Screen const&) = delete;
    Screen& operator=(Screen const&) = delete;
};

}

#endif
