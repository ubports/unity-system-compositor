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

#include "dbus_powerkey.h"

#include <QDBusMessage>
#include <QDBusConnection>

DBusPowerKey::DBusPowerKey(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.registerObject("/com/canonical/Unity/PowerKey", this);
    bus.registerService("com.canonical.Unity.PowerKey");
}

void DBusPowerKey::emit_power_off_request()
{
    QDBusMessage message = QDBusMessage::createSignal("/com/canonical/Unity/PowerKey",
        "com.canonical.Unity.PowerKey", "SystemPowerOffRequest");

    QDBusConnection bus = QDBusConnection::systemBus();
    bus.send(message);
}
