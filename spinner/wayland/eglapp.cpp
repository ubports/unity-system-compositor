/*
 * Copyright © 2013-2018 Canonical Ltd.
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
 */

#include "eglapp.h"

#include "waylandegl.h"

#include <wayland-client.h>

std::vector<std::shared_ptr<EglSurface>> wayland_eglapp_init(int argc, char *argv[])
{
    // TODO: add options
    (void) argc;
    (void) argv;

    struct wl_display* display = wl_display_connect(NULL);
    auto const mir_egl_app = make_wayland_eglapp(display);

    return wayland_surface_init(mir_egl_app);
}
