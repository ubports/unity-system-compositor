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

#include "screen_state_handler.h"
#include "dbus_screen.h"
#include "powerd_mediator.h"

#include <mir/compositor/compositor.h>
#include <mir/default_server_configuration.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_configuration.h>

#include <thread>
#include <condition_variable>

namespace mi = mir::input;
namespace mc = mir::compositor;
namespace mg = mir::graphics;

class OneshotTimer
{
public:
    //Callback is not allowed to call methods in this timer functor
    OneshotTimer(int initial_timer_value, std::function<void()> const& cb)
        : shutdown{false},
          run_timer{false},
          cancel_notification{false},
          timer_value{initial_timer_value},
          notify_timer_event{cb}
    {
        thread = std::thread(&OneshotTimer::run, this);
    }

    ~OneshotTimer()
    {
        stop();
        if (thread.joinable())
            thread.join();
    }

    void start(int seconds)
    {
        std::unique_lock<std::mutex> lock{timer_mutex};

        //Calling start during a pending timer is essentially a reset
        if (run_timer)
            cancel_l();

        //Wait until any pending timers have been canceled
        timer_ready_condition.wait(lock, [&]{ return !run_timer;});

        timer_value = seconds;
        run_timer = true;
        run_timer_condition.notify_one();
    }

    void cancel()
    {
        std::unique_lock<std::mutex> lock{timer_mutex};
        cancel_l();
    }

    void reset()
    {
        start(timer_value);
    }

private:
    void run() noexcept
    {
       std::unique_lock<std::mutex> lock{timer_mutex};
       while (!shutdown)
       {
           run_timer = false;
           timer_ready_condition.notify_all();

           //This is one-shot timer, so wait until client signals timer to start again
           run_timer_condition.wait(lock, [&]{ return run_timer || shutdown;});

           if (!shutdown)
           {
               cancel_notification = false;
               std::chrono::seconds duration(timer_value);
               timer.wait_for(lock, duration);
               if (!cancel_notification)
               {
                   lock.unlock();
                   notify_timer_event();
                   lock.lock();
               }
           }
       }
    }

    void stop()
    {
        std::unique_lock<std::mutex> lock{timer_mutex};

        shutdown = true;
        run_timer = false;
        cancel_l();
        run_timer_condition.notify_one();
    }

    void cancel_l()
    {
        cancel_notification = true;
        //Could be in the middle of waiting for timeout, signal to cancel timer
        timer.notify_one();
    }

    bool shutdown;
    bool run_timer;
    bool cancel_notification;
    std::mutex timer_mutex;
    std::condition_variable timer;
    std::condition_variable run_timer_condition;
    std::condition_variable timer_ready_condition;
    int timer_value;
    std::function<void()> notify_timer_event;
    std::thread thread;
};

ScreenStateHandler::ScreenStateHandler(std::shared_ptr<mir::DefaultServerConfiguration> const& config,
                                       int power_off_timeout,
                                       int dimmer_timeout)
    : power_off_timer{new OneshotTimer(power_off_timeout, [&]{ power_off_timer_notification(); })},
      dimmer_timer{new OneshotTimer(dimmer_timeout, [&]{ dimmer_timer_notification(); })},
      current_power_mode{MirPowerMode::mir_power_mode_on},
      power_off_delay{power_off_timeout},
      dimming_delay{dimmer_timeout},
      auto_brightness{true},
      dbus_screen{new DBusScreen([&](MirPowerMode m){ set_screen_power_mode(m);})},
      powerd_mediator{new PowerdMediator()},
      config{config}
{
    power_off_timer->start(power_off_delay);
    dimmer_timer->start(dimming_delay);
}

ScreenStateHandler::~ScreenStateHandler() = default;

bool ScreenStateHandler::handle(MirEvent const& event)
{
    static const int32_t POWER_KEY_CODE = 26;

    if (event.type == mir_event_type_key &&
        event.key.key_code == POWER_KEY_CODE &&
        event.key.action == mir_key_action_up)
    {
        toggle_screen_power_mode();
    }
    else if (event.type == mir_event_type_motion)
    {
        reset_timers();
        powerd_mediator->bright_backlight();
    }
    return false;
}

void ScreenStateHandler::set_timeouts(int power_off_timeout, int dimming_timeout)
{
    power_off_delay = power_off_timeout;
    dimming_delay = dimming_timeout;
}

void ScreenStateHandler::toggle_screen_power_mode()
{
    MirPowerMode new_mode = (current_power_mode == MirPowerMode::mir_power_mode_on) ?
            MirPowerMode::mir_power_mode_off : MirPowerMode::mir_power_mode_on;

    set_screen_power_mode(new_mode);
}

void ScreenStateHandler::set_screen_power_mode(MirPowerMode mode)
{
    if (current_power_mode == MirPowerMode::mir_power_mode_off)
    {
        set_display_power_mode(MirPowerMode::mir_power_mode_on);
        power_off_timer->start(power_off_delay);
        dimmer_timer->start(dimming_delay);
    }
    else
    {
        cancel_timers();
        set_display_power_mode(MirPowerMode::mir_power_mode_off);
    }
}

void ScreenStateHandler::set_display_power_mode(MirPowerMode mode)
{
    std::lock_guard<std::mutex> lock{power_mode_mutex};
    std::shared_ptr<mg::Display> display = config->the_display();
    std::shared_ptr<mg::DisplayConfiguration> displayConfig = display->configuration();
    std::shared_ptr<mc::Compositor> compositor = config->the_compositor();

    displayConfig->for_each_output(
        [&](const mg::UserDisplayConfigurationOutput displayConfigOutput) {
            displayConfigOutput.power_mode = mode;
        }
    );

    if (mode != MirPowerMode::mir_power_mode_on)
        compositor->stop();

    display->configure(*displayConfig.get());

    if (mode == MirPowerMode::mir_power_mode_on)
    {
        powerd_mediator->bright_backlight();
        compositor->start();
    }

    current_power_mode = mode;
    powerd_mediator->set_sys_state_for(mode);
    dbus_screen->emit_power_state_change(mode);
}

void ScreenStateHandler::power_off_timer_notification()
{
    set_display_power_mode(MirPowerMode::mir_power_mode_off);
}

void ScreenStateHandler::dimmer_timer_notification()
{
    powerd_mediator->dim_backlight();
}

void ScreenStateHandler::cancel_timers()
{
    power_off_timer->cancel();
    dimmer_timer->cancel();
}

void ScreenStateHandler::reset_timers()
{
    if (current_power_mode != MirPowerMode::mir_power_mode_off)
    {
        power_off_timer->start(power_off_delay);
        dimmer_timer->start(dimming_delay);
    }
}
