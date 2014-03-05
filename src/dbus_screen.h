/*
 * Copyright © 2013 Canonical Ltd.
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

namespace mir
{
    class DefaultServerConfiguration;
}

class DBusScreen : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.Screen")

public:
    explicit DBusScreen(std::shared_ptr<mir::DefaultServerConfiguration> config,
                        int off_delay, QObject *parent = 0);
    virtual ~DBusScreen();

public Q_SLOTS:
    bool setScreenPowerMode(const QString &mode);

private Q_SLOTS:
    void power_off();

private:
    std::shared_ptr<mir::DefaultServerConfiguration> config;
    QTimer *power_off_timer;
    MirPowerMode power_off_mode;
    int power_off_delay;
};

#endif /* DBUS_SCREEN_H_ */
