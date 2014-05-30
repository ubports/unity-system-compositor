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

#include <atomic>
#include <memory>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>

DBusScreen::DBusScreen(SetPowerModeFunc notify_power_mode,
    KeepDisplayOnFunc keep_display_on_func, QObject *parent)
    : QObject(parent),
      dbus_adaptor{new DBusScreenAdaptor(this)},
      service_watcher{new QDBusServiceWatcher()},
      notify_power_mode{notify_power_mode},
      keep_display_on{keep_display_on_func}
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.registerObject("/com/canonical/Unity/Screen", this);
    bus.registerService("com.canonical.Unity.Screen");

    service_watcher->setConnection(bus);
    service_watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(service_watcher.get(),
            SIGNAL(serviceUnregistered(QString const&)),
            this, SLOT(remove_display_on_requestor(QString const&)));

    //QT's ~QObject will release this child object
    //so release ownership from the unique_ptr
    dbus_adaptor.release();
}

DBusScreen::~DBusScreen() = default;

bool DBusScreen::setScreenPowerMode(const QString &mode, int reason)
{
    if (reason < Reason::normal || reason >= Reason::max_reasons)
        return false;

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

    notify_power_mode(newPowerMode, static_cast<Reason>(reason));

    return true;
}

void DBusScreen::emit_power_state_change(MirPowerMode power_mode, Reason reason)
{
    QDBusMessage message =  QDBusMessage::createSignal("/com/canonical/Unity/Screen",
        "com.canonical.Unity.Screen", "DisplayPowerStateChange");

    int power_state = (power_mode == MirPowerMode::mir_power_mode_off) ? 0 : 1;

    QVariant state(power_state);
    QList<QVariant> arguments;
    arguments.append(state);
    arguments.append(static_cast<int>(reason));
    message.setArguments(arguments);

    QDBusConnection bus = QDBusConnection::systemBus();
    bus.send(message);
}

int DBusScreen::keepDisplayOn()
{
    static std::atomic<uint32_t> request_id{0};

    int id = request_id.fetch_add(1);
    keep_display_on(true);

    auto const& caller = message().service();
    auto& caller_requests = display_requests[caller.toStdString()];

    if (caller_requests.size() == 0)
        service_watcher->addWatchedService(caller);

    caller_requests.insert(id);
    return id;
}

void DBusScreen::removeDisplayOnRequest(int cookie)
{
    auto const& requestor = message().service();

    auto it = display_requests.find(requestor.toStdString());
    if (it == display_requests.end())
        return;

    auto caller_requests = it->second;
    caller_requests.erase(cookie);
    if (caller_requests.size() == 0)
        remove_display_on_requestor(requestor);
}

void DBusScreen::remove_display_on_requestor(QString const& requestor)
{
    display_requests.erase(requestor.toStdString());
    service_watcher->removeWatchedService(requestor);
    if (display_requests.size() == 0)
        keep_display_on(false);
}
