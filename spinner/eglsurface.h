/*
 * Copyright © 2019 UBports project.
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

#ifndef UNITYSYSTEMCOMPOSITOR_EGLSURFACE_H
#define UNITYSYSTEMCOMPOSITOR_EGLSURFACE_H

#include <memory>
#include <functional>

class EglSurface
{
public:
    virtual void paint(std::function<void(unsigned int, unsigned int)> const& functor) = 0;
};

#endif //UNITYSYSTEMCOMPOSITOR_EGLSURFACE_H
