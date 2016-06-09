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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/mir_screen.h"

#include "usc/test/mock_display.h"

#include <mir/compositor/compositor.h>
#include <mir/graphics/display_configuration.h>
#include <mir/graphics/gl_context.h>

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

struct AMirScreen : testing::Test
{
    void turn_screen_off()
    {
        mir_screen.turn_off();
        verify_and_clear_expectations();
    }

    void turn_screen_on()
    {
        mir_screen.turn_on();
        verify_and_clear_expectations();
    }

    void verify_and_clear_expectations()
    {
        Mock::VerifyAndClearExpectations(display.get());
        Mock::VerifyAndClearExpectations(compositor.get());
    }

    std::shared_ptr<MockCompositor> compositor{
        std::make_shared<testing::NiceMock<MockCompositor>>()};
    std::shared_ptr<ut::MockDisplay> display{
        std::make_shared<testing::NiceMock<ut::MockDisplay>>()};

    usc::MirScreen mir_screen{
        compositor,
        display};
};

}

TEST_F(AMirScreen, stops_compositing_and_turns_off_display_when_turning_off)
{
    InSequence s;
    EXPECT_CALL(*compositor, stop());
    EXPECT_CALL(*display, configure(_));

    turn_screen_off();
}

TEST_F(AMirScreen, starts_compositing_and_turns_on_display_when_turning_on)
{
    turn_screen_off();

    InSequence s;
    EXPECT_CALL(*compositor, stop());
    EXPECT_CALL(*display, configure(_));
    EXPECT_CALL(*compositor, start());

    turn_screen_on();
}
