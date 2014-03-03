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

#include "dbus_screen.h"
#include "dbus_screen_adaptor.h"

#include <mir/compositor/compositor.h>
#include <mir/default_server_configuration.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>

#include <QDBusConnection>
#include <QDebug>

namespace mc = mir::compositor;
namespace mg = mir::graphics;

static void set_screen_power_mode(MirPowerMode mode, const std::shared_ptr<mir::DefaultServerConfiguration>& config)
{
    std::shared_ptr<mg::Display> display = config->the_display();
    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();
    std::shared_ptr<mc::Compositor> compositor = config->the_compositor();

    displayConfig->for_each_output(
        [&](const mg::UserDisplayConfigurationOutput displayConfigOutput) {
            displayConfigOutput.power_mode=mode;
        }
    );

    if (mode != MirPowerMode::mir_power_mode_on)
        compositor->stop();

    display->configure(*displayConfig.get());

    if (mode == MirPowerMode::mir_power_mode_on)
        compositor->start();
}

// Note: this class should be created only after when the Mir DisplayServer has started
DBusScreen::DBusScreen(std::shared_ptr<mir::DefaultServerConfiguration> config,
                       int off_delay, QObject *parent)
    : QObject(parent),
      config(config),
      power_off_timer(off_delay > 0 ? new QTimer(this) : nullptr),
      power_off_mode(MirPowerMode::mir_power_mode_off),
      power_off_delay(off_delay)

{
    new DBusScreenAdaptor(this);
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.registerObject("/com/canonical/Unity/Screen", this);
    bus.registerService("com.canonical.Unity.Screen");

    if (power_off_timer != nullptr) {
        power_off_timer->setSingleShot(true);
        connect(power_off_timer, SIGNAL(timeout()), this, SLOT(power_off()));
    }
}

DBusScreen::~DBusScreen()
{
    delete power_off_timer;
}

bool DBusScreen::setScreenPowerMode(const QString &mode)
{
    MirPowerMode newPowerMode;
    // Note: the "standby" and "suspend" modes are mostly unused

    if (mode == "on") {
        newPowerMode = MirPowerMode::mir_power_mode_on;
        if (power_off_timer != nullptr && power_off_timer->isActive())
            power_off_timer->stop();
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

    if ((newPowerMode != MirPowerMode::mir_power_mode_on) && (power_off_timer != nullptr)) {
        power_off_mode = newPowerMode;
        power_off_timer->start(power_off_delay);
    } else {
        set_screen_power_mode(newPowerMode, config);
    }

    return true;
}

void DBusScreen::power_off()
{
    set_screen_power_mode(power_off_mode, config);
}
