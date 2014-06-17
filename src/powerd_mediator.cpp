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
#include <QDBusMetaType>
#include <QDBusArgument>

#include <iostream>

/* Struct for reply to getBrightnessParams DBus call */
struct BrightnessParams
 {
    int dim_brightness;
    int min_brightness;
    int max_brightness;
    int default_brightness;
    bool auto_brightness_supported;
};
Q_DECLARE_METATYPE(BrightnessParams);

/* Not used but required by qDBusRegisterMetaType*/
QDBusArgument &operator<<(QDBusArgument &argument, const BrightnessParams &mystruct)
{
    argument.beginStructure();
    argument << mystruct.dim_brightness <<
                mystruct.min_brightness <<
                mystruct.max_brightness <<
                mystruct.default_brightness <<
                mystruct.auto_brightness_supported;
    argument.endStructure();
    return argument;
}

/* Needed to un-marshall powerd response */
const QDBusArgument &operator>>(const QDBusArgument &argument, BrightnessParams &mystruct)
{
    argument.beginStructure();
    argument >> mystruct.dim_brightness >>
                mystruct.min_brightness >>
                mystruct.max_brightness >>
                mystruct.default_brightness >>
                mystruct.auto_brightness_supported;
    argument.endStructure();
    return argument;
}

PowerdMediator::PowerdMediator()
    : dim_brightness{10},
      normal_brightness{102},
      current_brightness{0},
      min_brightness_{10},
      max_brightness_{102},
      auto_brightness_supported_{false},
      auto_brightness_enabled{false},
      auto_brightness_requested{false},
      acquired_sys_state{false},
      powerd_interface{new QDBusInterface("com.canonical.powerd",
                                          "/com/canonical/powerd",
                                          "com.canonical.powerd",
                                          QDBusConnection::systemBus())}
{
    if (!powerd_interface->isValid())
    {
        std::cerr << "Interface to powerd is invalid - last error: ";
        std::cerr << powerd_interface->lastError().message().toStdString();
        std::cerr << std::endl;
    }

    qDBusRegisterMetaType<BrightnessParams>();
    QDBusReply<BrightnessParams> reply = powerd_interface->call("getBrightnessParams");
    if (reply.isValid())
    {
        auto const& params = reply.value();
        dim_brightness = params.dim_brightness;
        normal_brightness = params.default_brightness;
        min_brightness_ = params.min_brightness;
        max_brightness_ = params.max_brightness;
        auto_brightness_supported_ = params.auto_brightness_supported;
    }
    else
    {
        std::cerr << "getBrightnessParams call failed with: ";
        std::cerr << reply.error().message().toStdString() << std::endl;
    }
}

PowerdMediator::~PowerdMediator() = default;

void PowerdMediator::set_dim_backlight()
{
    auto_brightness_enabled = false;
    apply_brightness(dim_brightness);
}

void PowerdMediator::set_normal_backlight()
{
    if (auto_brightness_requested)
        enable_auto_brightness(true);
    else
        apply_brightness(normal_brightness);
}

void PowerdMediator::turn_off_backlight()
{
    apply_brightness(0);
}

void PowerdMediator::change_backlight_values(int dim, int normal)
{
    if (normal >= min_brightness_ && normal <= max_brightness_)
        normal_brightness = normal;
    if (dim >= min_brightness_ && dim <= max_brightness_)
        dim_brightness = dim;
}

void PowerdMediator::enable_auto_brightness(bool enable)
{
    if (auto_brightness_supported_ && enable && !auto_brightness_enabled)
    {
        powerd_interface->call("userAutobrightnessEnable", true);
        auto_brightness_enabled = true;
        auto_brightness_requested = true;
        current_brightness = normal_brightness;
    }
    else if (auto_brightness_enabled && !enable)
    {
        auto_brightness_enabled = false;
        auto_brightness_requested = false;
        apply_brightness(normal_brightness);
    }
}

bool PowerdMediator::auto_brightness_supported()
{
    return auto_brightness_supported_;
}

int PowerdMediator::min_brightness()
{
    return min_brightness_;
}

int PowerdMediator::max_brightness()
{
    return max_brightness_;
}

void PowerdMediator::set_brightness(int brightness)
{
    normal_brightness = brightness;

    if (!auto_brightness_enabled)
        apply_brightness(brightness);
}

void PowerdMediator::apply_brightness(int brightness)
{
    if (current_brightness == brightness)
        return;

    powerd_interface->call("setUserBrightness", brightness);
    current_brightness = brightness;
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
