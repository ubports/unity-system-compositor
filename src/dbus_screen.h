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

#ifndef DBUS_SCREEN_H_
#define DBUS_SCREEN_H_

#include <mir_toolkit/common.h>

#include <memory>
#include <QObject>
#include <QtCore>

class QDBusInterface;

class DBusScreen : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.Screen")

public:
    enum Reason
    {
        normal = 0,
        inactivity = 1,
        power_key = 2,
        proximity = 3,
        max_reasons
    };
    typedef std::function<void(MirPowerMode mode, Reason reason)> Callback;
    explicit DBusScreen(Callback cb, QObject *parent = 0);
    void emit_power_state_change(MirPowerMode mode, Reason reason);

public Q_SLOTS:
    bool setScreenPowerMode(const QString &mode, int reason);

private:
    Callback notify_power_mode;

};

#endif /* DBUS_SCREEN_H_ */
