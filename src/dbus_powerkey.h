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

#ifndef DBUS_POWERKEY_H_
#define DBUS_POWERKEY_H_

#include <QObject>
#include <QtCore>

class QDBusInterface;

class DBusPowerKey : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.PowerKey")

public:
    explicit DBusPowerKey(QObject *parent = 0);
    void emit_power_off_request();
};

#endif /* DBUS_SCREEN_H_ */
