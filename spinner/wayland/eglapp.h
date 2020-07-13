/*
 * Copyright © 2013 Canonical Ltd.
 *             2019 UBports project
 *
 * This program is free software: you can redistribute it and/or modify
 * under the terms of the GNU General Public License version 2 or 3 as
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
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#ifndef __WAYLNDEGLAPP_H__
#define __WAYLNDEGLAPP_H__

#include <memory>
#include <vector>

class EglSurface;

std::vector<std::shared_ptr<EglSurface>> wayland_eglapp_init(int argc, char *argv[]);

#endif
