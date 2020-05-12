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

#include "../eglsurface.h"

#include <mir_toolkit/common.h>
#include <mir_toolkit/client_types.h>
#include "mir_toolkit/mir_client_library.h"

#include <EGL/egl.h>

#include <memory>
#include <functional>

class MirEglApp;
class MirEglSurface;

std::shared_ptr<MirEglApp> make_mir_eglapp(
    MirConnection* const connection,
    MirPixelFormat const& pixel_format);

class MirEglSurface : public EglSurface
{
public:
    MirEglSurface(
        std::shared_ptr<MirEglApp> const& mir_egl_app,
        MirWindowParameters const& surfaceparm,
        int swapinterval);

    ~MirEglSurface();

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

    std::shared_ptr<MirEglApp> const mir_egl_app;
    MirWindow* const surface;
    EGLSurface const eglsurface;
    int width_;
    int height_;
};

#endif //UNITYSYSTEMCOMPOSITOR_MIREGL_H
