/*
 * Copyright © 2014-2015 Canonical Ltd.
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

#ifndef POWERD_MEDIATOR_
#define POWERD_MEDIATOR_

#include "screen_hardware.h"

#include <mir_toolkit/common.h>

#include <QString>
#include <QObject>

#include <memory>
#include <mutex>
#include <condition_variable>

class QDBusInterface;
class QDBusServiceWatcher;

/*
 * A Proxy to powerd. Note this class is not thread-safe,
 * synchronization should be done externally.
 */
class PowerdMediator : public QObject, public usc::ScreenHardware
{
    Q_OBJECT
public:
    PowerdMediator();
    ~PowerdMediator();

    void set_dim_backlight() override;
    void set_normal_backlight() override;
    void turn_off_backlight() override;
    void allow_suspend() override;
    void disable_suspend() override;

    void change_backlight_values(int dim_brightness, int normal_brightness) override;
    void enable_auto_brightness(bool flag) override;

    bool auto_brightness_supported() override;
    int min_brightness() override;
    int max_brightness() override;

    void set_brightness(int brightness) override;

private Q_SLOTS:
    void powerd_registered();
    void powerd_state_changed(int state);

private:
    enum BacklightState
    {
        off,
        dim,
        normal,
        automatic
    };

    enum SystemState
    {
        unknown = -1,
        suspended = 0,
        active,
    };
    void change_backlight_state(BacklightState state, bool force_change = false);
    void init_brightness_params();
    bool request_suspend_blocker();
    void wait_for_state(SystemState state);

    int dim_brightness;
    int normal_brightness;
    int current_brightness;
    int min_brightness_;
    int max_brightness_;
    bool auto_brightness_supported_;
    bool auto_brightness_requested;
    BacklightState backlight_state;

    QString suspend_block_cookie;
    bool pending_suspend_blocker_request;

    std::unique_ptr<QDBusInterface> powerd_interface;
    std::unique_ptr<QDBusServiceWatcher> service_watcher;

    SystemState system_state;

    std::mutex system_state_mutex;
    std::condition_variable state_change;
};
#endif
