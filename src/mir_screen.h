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

#ifndef USC_MIR_SCREEN_H_
#define USC_MIR_SCREEN_H_

#include "screen.h"

#include <chrono>
#include <memory>
#include <mutex>

namespace mir
{
namespace compositor { class Compositor; }
namespace graphics {class Display;}
}

namespace usc
{

class MirScreen: public Screen
{
public:
    MirScreen(std::shared_ptr<mir::compositor::Compositor> const& compositor,
              std::shared_ptr<mir::graphics::Display> const& display);
    ~MirScreen();

    void turn_on();
    void turn_off();

private:
    void set_power_mode(MirPowerMode mode);

    std::shared_ptr<mir::compositor::Compositor> const compositor;
    std::shared_ptr<mir::graphics::Display> const display;

    MirPowerMode current_power_mode;
};

}

#endif
