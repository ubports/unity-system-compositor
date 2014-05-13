/*
 * Copyright © 2013-2014 Canonical Ltd.
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

#include "dbus_screen.h"
#include "dbus_screen_adaptor.h"

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDebug>

DBusScreen::DBusScreen(std::function<void(MirPowerMode)> cb, QObject *parent)
    : QObject(parent),
      notify_power_mode{cb}
{
    new DBusScreenAdaptor(this);
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.registerObject("/com/canonical/Unity/Screen", this);
    bus.registerService("com.canonical.Unity.Screen");
}

bool DBusScreen::setScreenPowerMode(const QString &mode)
{
    MirPowerMode newPowerMode;

    // Note: the "standby" and "suspend" modes are mostly unused
    if (mode == "on") {
        newPowerMode = MirPowerMode::mir_power_mode_on;
    } else if (mode == "standby") {
        newPowerMode = MirPowerMode::mir_power_mode_standby; // higher power "off" mode (fastest resume)
    } else if (mode == "suspend") {
        newPowerMode = MirPowerMode::mir_power_mode_suspend; // medium power "off" mode
    } else if (mode == "off") {
        newPowerMode = MirPowerMode::mir_power_mode_off;     // lowest power "off" mode (slowest resume)
    } else {
        qWarning() << "DBusScreen: unknown mode type" << mode;
        return false;
    }

    notify_power_mode(newPowerMode);

    return true;
}

void DBusScreen::emit_power_state_change(MirPowerMode power_mode)
{
    QDBusMessage message =  QDBusMessage::createSignal("/com/canonical/Unity/Screen",
        "com.canonical.Unity.Screen", "DisplayPowerStateChange");

    int power_state = (power_mode == MirPowerMode::mir_power_mode_off) ? 0 : 1;
    QVariant state(power_state);
    QList<QVariant> arguments;
    arguments.append(state);
    message.setArguments(arguments);

    QDBusConnection bus = QDBusConnection::systemBus();
    bus.send(message);
}
