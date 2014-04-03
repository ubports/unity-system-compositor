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

#include "powerd_mediator.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusPendingReply>

PowerdMediator::PowerdMediator()
    : dim_brightness{0},
      normal_brightness{100},
      last_brightness{-1},
      acquired_sys_state{false},
      powerd_interface{new QDBusInterface("com.canonical.powerd",
                                          "/com/canonical/powerd",
                                          "com.canonical.powerd",
                                          QDBusConnection::systemBus())}
{
    QDBusPendingReply<int, int, int, bool> reply =
        powerd_interface->asyncCall("getBrightnessParams");
    reply.waitForFinished();
    if (!reply.isError())
    {
        dim_brightness = reply.argumentAt<0>();
        normal_brightness = reply.argumentAt<2>();
    }
}

PowerdMediator::~PowerdMediator() = default;

void PowerdMediator::dim_backlight()
{
    if (last_brightness == dim_brightness) return;

    powerd_interface->call("setUserBrightness", dim_brightness);
    last_brightness = dim_brightness;
}

void PowerdMediator::bright_backlight()
{
    if (last_brightness == normal_brightness) return;

    powerd_interface->call("setUserBrightness", normal_brightness);
    last_brightness = normal_brightness;
}

void PowerdMediator::change_backlight_defaults(int dim, int bright)
{
    normal_brightness = bright;
    dim_brightness = dim;
}

void PowerdMediator::set_sys_state_for(MirPowerMode mode)
{
    if (mode == MirPowerMode::mir_power_mode_off)
        release_sys_state();
    else
        acquire_sys_state();
}

void PowerdMediator::release_sys_state()
{
    if (acquired_sys_state)
        powerd_interface->call("clearSysState", sys_state_cookie);
    acquired_sys_state = false;
}

void PowerdMediator::acquire_sys_state()
{
    if (!acquired_sys_state)
    {
        QDBusReply<QString> reply = powerd_interface->call("requestSysState", "com.canonical.Unity.Screen", 1);
        if (reply.isValid())
        {
            sys_state_cookie = reply.value();
            acquired_sys_state = true;
        }
    }
}
