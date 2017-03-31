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
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */


#include "src/mir_input_configuration.h"
#include "mir/input/input_device_hub.h"
#include "mir/input/device_capability.h"
#include "mir/input/input_device_observer.h"
#include "mir/input/device.h"
#include "mir/input/mir_keyboard_config.h"
#include "mir/input/mir_pointer_config.h"
#include "mir/input/mir_touchpad_config.h"
#include <mir/version.h>

#if MIR_SERVER_VERSION >= MIR_VERSION_NUMBER(0, 27, 0)
#include <mir/input/mir_touchscreen_config.h>
#endif

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mi = mir::input;

using namespace ::testing;

namespace
{
struct MockDevice : mi::Device
{
    MOCK_CONST_METHOD0(id, MirInputDeviceId());
    MOCK_CONST_METHOD0(capabilities, mi::DeviceCapabilities());
    MOCK_CONST_METHOD0(name, std::string());
    MOCK_CONST_METHOD0(unique_id, std::string());
    MOCK_CONST_METHOD0(pointer_configuration, mir::optional_value<MirPointerConfig>());
    MOCK_METHOD1(apply_pointer_configuration, void(MirPointerConfig const&));
    MOCK_CONST_METHOD0(touchpad_configuration, mir::optional_value<MirTouchpadConfig>());
    MOCK_METHOD1(apply_touchpad_configuration, void(MirTouchpadConfig const&));
    MOCK_CONST_METHOD0(keyboard_configuration, mir::optional_value<MirKeyboardConfig>());
    MOCK_METHOD1(apply_keyboard_configuration, void(MirKeyboardConfig const&));

    MockDevice(mi::DeviceCapabilities caps)
        : caps(caps)
    {
        ON_CALL(*this, capabilities()).WillByDefault(Return(this->caps));
    }

#if MIR_SERVER_VERSION >= MIR_VERSION_NUMBER(0, 27, 0)
    mir::optional_value<MirTouchscreenConfig> touchscreen_configuration() const { return {}; }
    void apply_touchscreen_configuration(MirTouchscreenConfig const&) {}
#endif

    mi::DeviceCapabilities caps;
};

struct MockInputDeviceHub : mi::InputDeviceHub
{
    MOCK_METHOD1(add_observer,void(std::shared_ptr<mi::InputDeviceObserver> const&));
    MOCK_METHOD1(remove_observer,void(std::weak_ptr<mi::InputDeviceObserver> const&));
    MOCK_METHOD1(for_each_input_device, void(std::function<void(mi::Device const&)> const&));
    MOCK_METHOD1(for_each_mutable_input_device, void(std::function<void(mi::Device&)> const&));
};

struct MirInputConfiguration : ::testing::Test
{
    template<typename T>
    using shared_mock = std::shared_ptr<::testing::NiceMock<T>>;
    shared_mock<MockInputDeviceHub> mock_hub{std::make_shared<::testing::NiceMock<MockInputDeviceHub>>()};
    shared_mock<MockDevice> mock_mouse{
        std::make_shared<::testing::NiceMock<MockDevice>>(mi::DeviceCapability::pointer)};
    shared_mock<MockDevice> mock_touchpad{std::make_shared<::testing::NiceMock<MockDevice>>(
        mi::DeviceCapability::pointer | mi::DeviceCapability::touchpad)};
    shared_mock<MockDevice> mock_keyboard{std::make_shared<::testing::NiceMock<MockDevice>>(
        mi::DeviceCapability::keyboard | mi::DeviceCapability::alpha_numeric)};

    std::shared_ptr<mi::InputDeviceObserver> obs;

    MirInputConfiguration()
    {
        ON_CALL(*mock_hub, add_observer(_)).WillByDefault(SaveArg<0>(&obs));
    }
};
}

TEST_F(MirInputConfiguration, registers_something_as_device_observer)
{
    EXPECT_CALL(*mock_hub, add_observer(_));

    usc::MirInputConfiguration config(mock_hub);
}

TEST_F(MirInputConfiguration, configures_device_on_add)
{
    usc::MirInputConfiguration config(mock_hub);

    EXPECT_CALL(*mock_touchpad, apply_pointer_configuration(_));
    EXPECT_CALL(*mock_touchpad, apply_touchpad_configuration(_));
    obs->device_added(mock_touchpad);
}

TEST_F(MirInputConfiguration, configures_mouse_on_add)
{
    usc::MirInputConfiguration config(mock_hub);

    EXPECT_CALL(*mock_mouse, apply_pointer_configuration(_));
    obs->device_added(mock_mouse);
}

TEST_F(MirInputConfiguration, ignores_keyboard_when_added)
{
    usc::MirInputConfiguration config(mock_hub);

    EXPECT_CALL(*mock_keyboard, apply_touchpad_configuration(_)).Times(0);
    EXPECT_CALL(*mock_keyboard, apply_pointer_configuration(_)).Times(0);
    obs->device_added(mock_keyboard);
}

