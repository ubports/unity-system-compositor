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
#include "dbus_screen_observer.h"
#include "power_state_change_reason.h"
#include "worker_thread.h"

#include <atomic>
#include <memory>
#include <thread>
#include <iostream>

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>

namespace
{
bool is_valid_reason(int raw_reason)
{
    auto reason = static_cast<PowerStateChangeReason>(raw_reason);
    switch (reason)
    {
    case PowerStateChangeReason::unknown:
    case PowerStateChangeReason::inactivity:
    case PowerStateChangeReason::power_key:
    case PowerStateChangeReason::proximity:
        return true;
    }
    return false;
}

enum DBusHandlerTaskId
{
    set_power_mode
};
}


DBusScreen::DBusScreen(DBusScreenObserver& observer, QObject *parent)
    : QObject(parent),
      dbus_adaptor{new DBusScreenAdaptor(this)},
      service_watcher{new QDBusServiceWatcher()},
      observer{&observer},
      worker_thread{new usc::WorkerThread("USC/DBusHandler")}
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
    if (!is_valid_reason(reason))
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

    //This call may block - avoid blocking this dbus handling thread
    worker_thread->queue_task([this, newPowerMode, reason]{
        observer->set_screen_power_mode(newPowerMode, static_cast<PowerStateChangeReason>(reason));
    }, DBusHandlerTaskId::set_power_mode);

    return true;
}

void DBusScreen::emit_power_state_change(MirPowerMode power_mode, PowerStateChangeReason reason)
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
    auto const& caller = message().service();

    worker_thread->queue_task([this, id, caller]{
        std::lock_guard<decltype(guard)> lock{guard};

        //Check that the owner of the request is still valid
        auto system_bus_if = QDBusConnection::systemBus().interface();
        QDBusReply<QString> reply = system_bus_if->serviceOwner(caller);
        if (!reply.isValid())
            return;

        auto& caller_requests = display_requests[caller.toStdString()];

        if (caller_requests.size() == 0)
            service_watcher->addWatchedService(caller);

        caller_requests.insert(id);

        //This call may block so it needs to be executed outside the thread
        //that received the dbus call
        observer->keep_display_on(true);

        std::cout << "keepDisplayOn request id:" << id;
        std::cout << " requested by \"" << caller.toStdString() << "\"" << std::endl;
    });
    return id;
}

void DBusScreen::removeDisplayOnRequest(int cookie)
{
    std::lock_guard<decltype(guard)> lock{guard};
    auto const& requestor = message().service();

    auto it = display_requests.find(requestor.toStdString());
    if (it == display_requests.end())
        return;

    std::cout << "removeDisplayOnRequest id:" << cookie;
    std::cout << " requested by \"" << requestor.toStdString() << "\"" << std::endl;

    auto caller_requests = it->second;
    caller_requests.erase(cookie);
    if (caller_requests.size() == 0)
        remove_requestor(requestor, lock);
}

void DBusScreen::remove_display_on_requestor(QString const& requestor)
{
    std::lock_guard<decltype(guard)> lock{guard};
    remove_requestor(requestor, lock);
}

void DBusScreen::remove_requestor(QString const& requestor, std::lock_guard<std::mutex> const&)
{
    std::cout << "remove_display_on_requestor \"" << requestor.toStdString() << "\"";
    std::cout << std::endl;

    display_requests.erase(requestor.toStdString());
    service_watcher->removeWatchedService(requestor);
    if (display_requests.size() == 0)
        observer->keep_display_on(false);
}

void DBusScreen::setUserBrightness(int brightness)
{
    observer->set_brightness(brightness);
}

void DBusScreen::userAutobrightnessEnable(bool enable)
{
    observer->enable_auto_brightness(enable);
}

void DBusScreen::setInactivityTimeouts(int poweroff_timeout, int dimmer_timeout)
{
    observer->set_inactivity_timeouts(poweroff_timeout, dimmer_timeout);
}
