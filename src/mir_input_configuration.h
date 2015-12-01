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

#ifndef USC_MIR_INPUT_CONFIGRATION_H_
#define USC_MIR_INPUT_CONFIGRATION_H_

#include "input_configuration.h"

#include "mir/input/touchpad_configuration.h"
#include "mir/input/pointer_configuration.h"

#include <memory>
#include <thread>
#include <mutex>
#include <unordered_set>

namespace mir
{
namespace input
{
class InputDeviceHub;
class Device;
class InputDeviceObserver;
}
}

namespace usc
{

struct MirInputConfiguration : InputConfiguration
{
public:
    MirInputConfiguration(std::shared_ptr<mir::input::InputDeviceHub> const& device_hub);
    void set_mouse_primary_button(int32_t button) override;
    void set_mouse_cursor_speed(double speed) override;
    void set_mouse_scroll_speed(double speed) override;
    void set_touchpad_primary_button(int32_t button) override;
    void set_touchpad_cursor_speed(double speed) override;
    void set_touchpad_scroll_speed(double speed) override;
    void set_two_finger_scroll(bool enable) override;
    void set_tap_to_click(bool enable) override;
    void set_disable_touchpad_while_typing(bool enable) override;
    void set_disable_touchpad_with_mouse(bool enable) override;

    void device_added(std::shared_ptr<mir::input::Device> const& device);
    void device_removed(std::shared_ptr<mir::input::Device> const& device);
private:
    void configure_mouse(mir::input::Device& dev);
    void configure_touchpad(mir::input::Device& dev);
    void update_touchpads();
    void update_mice();

    std::shared_ptr<mir::input::InputDeviceObserver> const observer;
    std::mutex devices_lock;
    std::unordered_set<std::shared_ptr<mir::input::Device>> touchpads;
    std::unordered_set<std::shared_ptr<mir::input::Device>> mice;
    mir::input::PointerConfiguration mouse_pointer_config;
    mir::input::PointerConfiguration touchpad_pointer_config;
    mir::input::TouchpadConfiguration touchpad_config;
};

}

#endif
