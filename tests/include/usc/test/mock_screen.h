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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef USC_TEST_MOCK_SCREEN_H_
#define USC_TEST_MOCK_SCREEN_H_

#include "src/screen.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace usc
{
namespace test
{

struct MockScreen : usc::Screen
{
    MOCK_METHOD1(turn_on, void(OutputFilter));
    MOCK_METHOD1(turn_off, void(OutputFilter));
    MOCK_METHOD2(register_active_outputs_handler, void(void *, ActiveOutputsHandler const&));
    MOCK_METHOD1(unregister_active_outputs_handler, void(void*));
};

}
}

#endif
