/*
 * Copyright © 2015-2018 Canonical Ltd.
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

#ifndef UNITYSYSTEMCOMPOSITOR_WAYLANDEGL_H
#define UNITYSYSTEMCOMPOSITOR_WAYLANDEGL_H

#include "../eglsurface.h"

#include <EGL/egl.h>

#include <memory>
#include <vector>
#include <functional>

class WaylandEglApp;
class EglSurface;

std::shared_ptr<WaylandEglApp> make_wayland_eglapp(struct wl_display* display);
std::vector<std::shared_ptr<EglSurface>> wayland_surface_init(std::shared_ptr<WaylandEglApp> const& app);

class WaylandEglSurface : public EglSurface
{
public:
    WaylandEglSurface(std::shared_ptr<WaylandEglApp> const& mir_egl_app, struct wl_output* wl_output, int width, int height);

    ~WaylandEglSurface();

    void paint(std::function<void(unsigned int, unsigned int)> const& functor) override
    {
        egl_make_current();
        functor(width(), height());
        swap_buffers();
    }

private:
    void egl_make_current();

    void swap_buffers();
    unsigned int width() const;
    unsigned int height() const;

    std::shared_ptr<WaylandEglApp> const mir_egl_app;

    void* content_area = nullptr;
    struct wl_display* display = nullptr;
    struct wl_surface* surface = nullptr;
    struct wl_callback* new_frame_signal = nullptr;
    struct wl_shell_surface* window = nullptr;
    struct Buffers
    {
        struct wl_buffer* buffer;
        bool available;
    } buffers[4];
    bool waiting_for_buffer = true;

    EGLSurface eglsurface;
    int width_;
    int height_;

    static void shell_surface_ping(void *data, struct wl_shell_surface *wl_shell_surface, uint32_t serial);

    static void shell_surface_configure(void *data,
        struct wl_shell_surface *wl_shell_surface,
        uint32_t edges,
        int32_t width,
        int32_t height);
    static void shell_surface_popup_done(void *data, struct wl_shell_surface *wl_shell_surface);

};

#endif //UNITYSYSTEMCOMPOSITOR_WAYLANDEGL_H