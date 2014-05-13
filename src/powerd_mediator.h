/*
 * Copyright © 2014 Canonical Ltd.
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

#ifndef POWERD_MEDIATOR_
#define POWERD_MEDIATOR_

#include <mir_toolkit/common.h>

#include <QString>
#include <memory>
#include <mutex>

class QDBusInterface;
/*
 * A Proxy to powerd. Note this class is not thread-safe,
 * synchronization should be done externally.
 */
class PowerdMediator
{
public:
    PowerdMediator();
    ~PowerdMediator();

    void set_dim_backlight();
    void set_normal_backlight();
    void turn_off_backlight();
    void set_sys_state_for(MirPowerMode mode);

    void change_backlight_values(int dim_brightness, int normal_brightness);
    void enable_auto_brightness(bool flag);

    bool auto_brightness_supported();
    int min_brightness();
    int max_brightness();

private:
    void set_brightness(int brightness);
    void release_sys_state();
    void acquire_sys_state();

    int dim_brightness;
    int normal_brightness;
    int current_brightness;
    int min_brightness_;
    int max_brightness_;
    bool auto_brightness_supported_;

    QString sys_state_cookie;
    bool acquired_sys_state;

    std::unique_ptr<QDBusInterface> powerd_interface;
};
#endif
