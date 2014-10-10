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
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include <QObject>
#include <QtCore>
#include <QDBusContext>

namespace usc {class WorkerThread;}

class DBusScreenAdaptor;
class DBusScreenObserver;
class QDBusInterface;
class QDBusServiceWatcher;
enum class PowerStateChangeReason;

class DBusScreen : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.Screen")

public:
    explicit DBusScreen(DBusScreenObserver& observer, QObject *parent = 0);
    virtual ~DBusScreen();

    void emit_power_state_change(MirPowerMode mode, PowerStateChangeReason reason);

public Q_SLOTS:
    bool setScreenPowerMode(const QString &mode, int reason);
    int keepDisplayOn();
    void removeDisplayOnRequest(int id);

    //TODO: Expose same DBus powerd interface for now
    void setUserBrightness(int brightness);
    void userAutobrightnessEnable(bool enable);

    void setInactivityTimeouts(int poweroff_timeout, int dimmer_timeout);
    
    void setTouchVisualizationEnabled(bool enabled);

private Q_SLOTS:
    void remove_display_on_requestor(QString const& requestor);

private:
    void remove_requestor(QString const& requestor, std::lock_guard<std::mutex> const& lock);

    std::mutex guard;
    std::unique_ptr<DBusScreenAdaptor> dbus_adaptor;
    std::unique_ptr<QDBusServiceWatcher> service_watcher;
    std::unordered_map<std::string, std::unordered_set<int>> display_requests;
    DBusScreenObserver* const observer;
    std::unique_ptr<usc::WorkerThread> worker_thread;
};

#endif /* DBUS_SCREEN_H_ */
