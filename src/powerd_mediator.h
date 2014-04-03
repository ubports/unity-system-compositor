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

class QDBusInterface;

class PowerdMediator
{
public:
    PowerdMediator();
    ~PowerdMediator();

    void dim_backlight();
    void bright_backlight();
    void set_sys_state_for(MirPowerMode mode);

    void change_backlight_defaults(int dim_brightness, int normal_brightness);


private:

    void release_sys_state();
    void acquire_sys_state();

    int dim_brightness;
    int normal_brightness;
    int last_brightness;
    QString sys_state_cookie;
    bool acquired_sys_state;
    std::unique_ptr<QDBusInterface> powerd_interface;
};
#endif
