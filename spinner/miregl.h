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

#ifndef UNITYSYSTEMCOMPOSITOR_MIREGL_H
#define UNITYSYSTEMCOMPOSITOR_MIREGL_H

#include <mir_toolkit/common.h>
#include <mir_toolkit/client_types.h>
#include "mir_toolkit/mir_client_library.h"

#include <EGL/egl.h>

#include <memory>

class MirEglApp;
class MirEglSurface;

std::shared_ptr<MirEglApp> make_mir_eglapp(
    MirConnection* const connection,
    MirPixelFormat const& pixel_format,
    EGLint swapinterval);

class MirEglSurface
{
public:
    MirEglSurface(std::shared_ptr<MirEglApp> const& mir_egl_app, MirSurfaceParameters const& surfaceparm);

    ~MirEglSurface();

    template<typename Painter>
    void paint(Painter const& functor)
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

    std::shared_ptr<MirEglApp> const mir_egl_app;
    MirSurface* const surface;
    MirBufferPackage* buffer_package;
    EGLSurface const eglsurface;
};

#endif //UNITYSYSTEMCOMPOSITOR_MIREGL_H
