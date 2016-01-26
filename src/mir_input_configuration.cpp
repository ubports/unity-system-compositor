/*
 * Copyright © 2015 Canonical Ltd.
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

#include "mir_input_configuration.h"
#include "mir/input/input_device_observer.h"
#include "mir/input/input_device_hub.h"
#include "mir/input/pointer_configuration.h"
#include "mir/input/touchpad_configuration.h"
#include "mir/input/device.h"

namespace mi = mir::input;

namespace
{
struct DeviceObserver : mi::InputDeviceObserver
{
    usc::MirInputConfiguration* conf;
    DeviceObserver(usc::MirInputConfiguration* conf)
        : conf{conf}
    {
    }

    void device_added(std::shared_ptr<mi::Device> const& device) override
    {
        conf->device_added(device);
    }

    void device_changed(std::shared_ptr<mi::Device> const&) override
    {
    }

    void device_removed(std::shared_ptr<mi::Device> const&device) override
    {
        conf->device_removed(device);
    }

    void changes_complete() override
    {
    }
};
}


usc::MirInputConfiguration::MirInputConfiguration(std::shared_ptr<mi::InputDeviceHub> const& device_hub) :
    observer{std::make_shared<DeviceObserver>(this)}
{
    device_hub->add_observer(observer);
}

void usc::MirInputConfiguration::device_removed(std::shared_ptr<mi::Device> const& dev)
{
    std::lock_guard<decltype(devices_lock)> lock(devices_lock);
    touchpads.erase(dev);
    mice.erase(dev);
}

void usc::MirInputConfiguration::configure_mouse(mi::Device& dev)
{
    dev.apply_pointer_configuration(mouse_pointer_config);
}

void usc::MirInputConfiguration::configure_touchpad(mi::Device& dev)
{
    dev.apply_pointer_configuration(touchpad_pointer_config);
    dev.apply_touchpad_configuration(touchpad_config);
}

void usc::MirInputConfiguration::device_added(std::shared_ptr<mi::Device> const& dev)
{
    std::lock_guard<decltype(devices_lock)> lock(devices_lock);

    if (contains(dev->capabilities(), mi::DeviceCapability::touchpad))
    {
        touchpads.insert(dev);
        configure_touchpad(*dev);
    }
    else if (contains(dev->capabilities(), mi::DeviceCapability::pointer))
    {
        mice.insert(dev);
        configure_mouse(*dev);
    }
}

void usc::MirInputConfiguration::update_touchpads()
{
    for (auto const& touchpad : touchpads)
        configure_touchpad(*touchpad);
}

void usc::MirInputConfiguration::update_mice()
{
    for (auto const& mouse : mice)
        configure_mouse(*mouse);
}

void usc::MirInputConfiguration::set_mouse_primary_button(int32_t button)
{
    mouse_pointer_config.handedness = button == 0 ?
        mir_pointer_handedness_right :
        mir_pointer_handedness_left;
    update_mice();
}

void usc::MirInputConfiguration::set_mouse_cursor_speed(double speed)
{
    double clamped = speed;
    if (clamped < 0.0)
        clamped = 0.0;
    if (clamped > 1.0)
        clamped = 1.0;
    mouse_pointer_config.cursor_acceleration_bias = clamped * 2.0 - 1.0;
    update_mice();
}

void usc::MirInputConfiguration::set_mouse_scroll_speed(double speed)
{
    mouse_pointer_config.horizontal_scroll_scale = speed;
    mouse_pointer_config.vertical_scroll_scale = speed;
    update_mice();
}

void usc::MirInputConfiguration::set_touchpad_primary_button(int32_t button)
{
    touchpad_pointer_config.handedness = button == 0?mir_pointer_handedness_right:mir_pointer_handedness_left;
    update_touchpads();
}

void usc::MirInputConfiguration::set_touchpad_cursor_speed(double speed)
{
    double clamped = speed;
    if (clamped < 0.0)
        clamped = 0.0;
    if (clamped > 1.0)
        clamped = 1.0;
    touchpad_pointer_config.cursor_acceleration_bias = clamped * 2.0 - 1.0;
    update_touchpads();
}

void usc::MirInputConfiguration::set_touchpad_scroll_speed(double speed)
{
    touchpad_pointer_config.horizontal_scroll_scale = speed;
    touchpad_pointer_config.vertical_scroll_scale = speed;
    update_touchpads();
}

void usc::MirInputConfiguration::set_two_finger_scroll(bool enable)
{
    MirTouchpadScrollModes current = touchpad_config.scroll_mode;
    if (enable)
        current |= mir_touchpad_scroll_mode_two_finger_scroll;
    else
        current &= ~mir_touchpad_scroll_mode_two_finger_scroll;
    touchpad_config.scroll_mode = current;
    update_touchpads();
}

void usc::MirInputConfiguration::set_tap_to_click(bool enable)
{
    touchpad_config.tap_to_click = enable;
    update_touchpads();
}

void usc::MirInputConfiguration::set_disable_touchpad_while_typing(bool enable)
{
    touchpad_config.disable_while_typing = enable;
    update_touchpads();
}

void usc::MirInputConfiguration::set_disable_touchpad_with_mouse(bool enable)
{
    touchpad_config.disable_with_mouse = enable;
    update_touchpads();
}

