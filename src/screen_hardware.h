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

#ifndef USC_SCREEN_HARDWARE_H_
#define USC_SCREEN_HARDWARE_H_

namespace usc
{

class ScreenHardware
{
public:
    virtual ~ScreenHardware() = default;

    virtual void set_dim_backlight() = 0;
    virtual void set_normal_backlight() = 0;
    virtual void turn_off_backlight() = 0;
    virtual void change_backlight_values(int dim_brightness, int normal_brightness) = 0;

    virtual void allow_suspend() = 0;
    virtual void disable_suspend() = 0;

    virtual void enable_auto_brightness(bool flag) = 0;
    virtual bool auto_brightness_supported() = 0;
    virtual void set_brightness(int brightness) = 0;
    virtual int min_brightness() = 0;
    virtual int max_brightness() = 0;

    virtual void enable_proximity(bool enable) = 0;

protected:
    ScreenHardware() = default;
    ScreenHardware(ScreenHardware const&) = delete;
    ScreenHardware& operator=(ScreenHardware const&) = delete;
};

}
#endif
