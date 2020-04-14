/*
 * Copyright © 2015 Canonical Ltd.
 * Copyright (C) 2020 UBports foundation.
 * Author(s): Ratchanan Srirattanamet <ratchanan@ubports.com>
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/mir_screen.h"

#include "usc/test/mock_display.h"
#include "usc/test/stub_display_configuration.h"
#include "fake_shared.h"

#include <mir/compositor/compositor.h>
#include <mir/graphics/display_configuration.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;

namespace mg = mir::graphics;
namespace ut = usc::test;

namespace
{

struct MockCompositor : mir::compositor::Compositor
{
    MOCK_METHOD0(start, void());
    MOCK_METHOD0(stop, void());
};

struct MockDisplayWithExternalOutputs : ut::MockDisplay
{
    std::unique_ptr<mir::graphics::DisplayConfiguration> configuration() const override
    {
        return std::make_unique<usc::test::StubDisplayConfiguration>(2, 3, 1);
    }
};

struct AMirScreen : testing::Test
{
    void turn_all_displays_off()
    {
        mir_screen->turn_off(usc::OutputFilter::all);
        verify_and_clear_expectations();
    }

    void turn_all_displays_on()
    {
        mir_screen->turn_on(usc::OutputFilter::all);
        verify_and_clear_expectations();
    }

    void turn_internal_displays_off()
    {
        mir_screen->turn_off(usc::OutputFilter::internal);
        verify_and_clear_expectations();
    }

    void verify_and_clear_expectations()
    {
        Mock::VerifyAndClearExpectations(display.get());
        Mock::VerifyAndClearExpectations(compositor.get());
    }

    void use_mir_screen_with_external_outputs()
    {
        display = std::make_shared<testing::NiceMock<MockDisplayWithExternalOutputs>>();
        mir_screen = std::make_shared<usc::MirScreen>(compositor, display);
    }

    std::shared_ptr<MockCompositor> compositor{
        std::make_shared<testing::NiceMock<MockCompositor>>()};
    std::shared_ptr<ut::MockDisplay> display{
        std::make_shared<testing::NiceMock<ut::MockDisplay>>()};

    usc::ActiveOutputs const config_active_outputs{1, 3};
    int const config_inactive_outputs = 2;
    ut::StubDisplayConfiguration stub_display_configuration{
        config_active_outputs.internal,
        config_active_outputs.external,
        config_inactive_outputs};

    usc::ActiveOutputs active_outputs{-1,-1};
    usc::ActiveOutputsHandler active_outputs_handler =
        [this] (usc::ActiveOutputs const& active_outputs_arg)
        {
            active_outputs = active_outputs_arg;
        };

    std::shared_ptr<usc::MirScreen> mir_screen{
        std::make_shared<usc::MirScreen>(compositor, display)};
};

}

TEST_F(AMirScreen, stops_compositing_and_turns_off_display_when_turning_off)
{
    InSequence s;
    EXPECT_CALL(*compositor, stop());
    EXPECT_CALL(*display, configure(_));

    turn_all_displays_off();
}

TEST_F(AMirScreen, starts_compositing_and_turns_on_display_when_turning_on)
{
    turn_all_displays_off();

    InSequence s;
    EXPECT_CALL(*compositor, stop());
    EXPECT_CALL(*display, configure(_));
    EXPECT_CALL(*compositor, start());

    turn_all_displays_on();
}

TEST_F(AMirScreen, stops_compositing_and_turns_off_internal_when_only_internal)
{
    InSequence s;
    EXPECT_CALL(*compositor, stop());
    EXPECT_CALL(*display, configure(_));

    turn_internal_displays_off();
}

TEST_F(AMirScreen, restarts_compositing_after_turn_off_internal_if_active_outputs_remain)
{
    use_mir_screen_with_external_outputs();

    InSequence s;
    EXPECT_CALL(*compositor, stop());
    EXPECT_CALL(*display, configure(_));
    EXPECT_CALL(*compositor, start());

    turn_internal_displays_off();
}

TEST_F(AMirScreen, registered_handler_is_called_immediately)
{
    mir_screen->register_active_outputs_handler(this, active_outputs_handler);

    EXPECT_THAT(active_outputs, Eq(usc::ActiveOutputs{1, 0}));
}

TEST_F(AMirScreen, initial_configuration_calls_handler)
{
    mir_screen->register_active_outputs_handler(this, active_outputs_handler);

    mir_screen->initial_configuration(ut::fake_shared(stub_display_configuration));

    EXPECT_THAT(active_outputs, Eq(config_active_outputs));
}

TEST_F(AMirScreen, configuration_applied_calls_handler)
{
    mir_screen->register_active_outputs_handler(this, active_outputs_handler);

    mir_screen->configuration_applied(ut::fake_shared(stub_display_configuration));

    EXPECT_THAT(active_outputs, Eq(config_active_outputs));
}

TEST_F(AMirScreen, turning_on_calls_handler)
{
    mir_screen->register_active_outputs_handler(this, active_outputs_handler);

    turn_all_displays_on();

    EXPECT_THAT(active_outputs, Eq(usc::ActiveOutputs{1, 0}));
}

TEST_F(AMirScreen, turning_off_calls_handler)
{
    mir_screen->register_active_outputs_handler(this, active_outputs_handler);

    turn_all_displays_off();

    EXPECT_THAT(active_outputs, Eq(usc::ActiveOutputs{0, 0}));
}

TEST_F(AMirScreen, support_multiple_handlers)
{
    bool another_handler_called = false;

    mir_screen->register_active_outputs_handler(this, active_outputs_handler);
    // Use a different object as an owner for additional handler
    mir_screen->register_active_outputs_handler(&display,
        [&another_handler_called](usc::ActiveOutputs const&)
            { another_handler_called = true; });

    EXPECT_TRUE(another_handler_called);

    another_handler_called = false;
    mir_screen->configuration_applied(ut::fake_shared(stub_display_configuration));
    EXPECT_TRUE(another_handler_called);

    mir_screen->unregister_active_outputs_handler(&display);
}
