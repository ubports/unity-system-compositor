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

#include "miregl.h"

#include <GLES2/gl2.h>

class MirEglApp
{
public:
    MirEglApp(MirConnection* const connection, MirPixelFormat pixel_format, EGLint swapinterval);

    EGLSurface create_surface(MirSurface* surface);

    void release_current();

    void make_current(EGLSurface eglsurface) const;

    void swap_buffers(EGLSurface eglsurface) const;

    void destroy_surface(EGLSurface eglsurface) const;

    ~MirEglApp();

    MirConnection* const connection;
private:
    EGLDisplay egldisplay;
    EGLContext eglctx;
    EGLConfig eglconfig;
    EGLint neglconfigs;
};

std::shared_ptr<MirEglApp> make_mir_eglapp(
    MirConnection* const connection, MirPixelFormat const& pixel_format, EGLint swapinterval)
{
    return std::make_shared<MirEglApp>(connection, pixel_format, swapinterval);
}

namespace
{
MirSurface* create_surface(MirConnection* const connection, MirSurfaceParameters const& surfaceparm)
{
    auto const spec = mir_connection_create_spec_for_normal_surface(
        connection,
        surfaceparm.width,
        surfaceparm.height,
        surfaceparm.pixel_format);

    mir_surface_spec_set_name(spec, surfaceparm.name);
    mir_surface_spec_set_buffer_usage(spec, surfaceparm.buffer_usage);
    mir_surface_spec_set_fullscreen_on_output(spec, surfaceparm.output_id);

    auto const surface = mir_surface_create_sync(spec);
    mir_surface_spec_release(spec);

    if (!mir_surface_is_valid(surface))
        throw std::runtime_error("Can't create a surface");

    if (surfaceparm.output_id != mir_display_output_id_invalid)
        mir_surface_set_state(surface, mir_surface_state_fullscreen);

    return surface;
}
}

MirEglSurface::MirEglSurface(std::shared_ptr<MirEglApp> const& mir_egl_app, MirSurfaceParameters const& surfaceparm) :
    mir_egl_app{mir_egl_app},
    surface{create_surface(mir_egl_app->connection, surfaceparm)},
    eglsurface{mir_egl_app->create_surface(surface)}
{
}

MirEglSurface::~MirEglSurface()
{
    mir_egl_app->destroy_surface(eglsurface);
    mir_surface_release_sync(surface);
}

void MirEglSurface::egl_make_current()
{
    auto const buffer_stream = mir_surface_get_buffer_stream(surface);
    mir_buffer_stream_get_current_buffer(buffer_stream, &buffer_package);
    mir_egl_app->make_current(eglsurface);
}

void MirEglSurface::swap_buffers()
{
    mir_egl_app->swap_buffers(eglsurface);
}

void MirEglSurface::egl_release_current()
{
    mir_egl_app->release_current();
}

unsigned int MirEglSurface::width() const
{
    return buffer_package->width;
}

unsigned int MirEglSurface::height() const
{
    return buffer_package->height;
}

MirEglApp::MirEglApp(MirConnection* const connection, MirPixelFormat pixel_format, EGLint swapinterval) :
    connection{connection}
{
    unsigned int bpp = 8*MIR_BYTES_PER_PIXEL(pixel_format);

    EGLint attribs[] =
        {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
            EGL_BUFFER_SIZE, (EGLint) bpp,
            EGL_NONE
        };

    egldisplay = eglGetDisplay((EGLNativeDisplayType) mir_connection_get_egl_native_display(connection));
    if (egldisplay == EGL_NO_DISPLAY)
        throw std::runtime_error("Can't eglGetDisplay");

    EGLint major;
    EGLint minor;
    if (!eglInitialize(egldisplay, &major, &minor))
        throw std::runtime_error("Can't eglInitialize");

    if (major != 1 || minor != 4)
        throw std::runtime_error("EGL version is not 1.4");

    if (!eglChooseConfig(egldisplay, attribs, &eglconfig, 1, &neglconfigs))
        throw std::runtime_error("Could not eglChooseConfig");

    if (neglconfigs == 0)
        throw std::runtime_error("No EGL config available");

    EGLint ctxattribs[] =
        {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };

    eglctx = eglCreateContext(egldisplay, eglconfig, EGL_NO_CONTEXT, ctxattribs);
    if (eglctx == EGL_NO_CONTEXT)
        throw std::runtime_error("eglCreateContext failed");

    if (!eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, eglctx))
        throw std::runtime_error("Can't eglMakeCurrent");

    eglSwapInterval(egldisplay, swapinterval);
}

EGLSurface MirEglApp::create_surface(MirSurface* surface)
{
    auto const eglsurface = eglCreateWindowSurface(
        egldisplay,
        eglconfig,
        (EGLNativeWindowType) mir_buffer_stream_get_egl_native_window(mir_surface_get_buffer_stream(surface)), NULL);

    if (eglsurface == EGL_NO_SURFACE)
        throw std::runtime_error("eglCreateWindowSurface failed");

    return eglsurface;
}

void MirEglApp::release_current()
{
    eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void MirEglApp::make_current(EGLSurface eglsurface) const
{
    if (!eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglctx))
        throw std::runtime_error("Can't eglMakeCurrent");
}

void MirEglApp::swap_buffers(EGLSurface eglsurface) const
{
    EGLint width;
    EGLint height;

    eglSwapBuffers(egldisplay, eglsurface);

    /*
     * Querying the surface (actually the current buffer) dimensions here is
     * the only truly safe way to be sure that the dimensions we think we
     * have are those of the buffer being rendered to. But this should be
     * improved in future; https://bugs.launchpad.net/mir/+bug/1194384
     */
    if (eglQuerySurface(egldisplay, eglsurface, EGL_WIDTH, &width) &&
        eglQuerySurface(egldisplay, eglsurface, EGL_HEIGHT, &height))
    {
        glViewport(0, 0, width, height);
    }
}

void MirEglApp::destroy_surface(EGLSurface eglsurface) const
{
    eglDestroySurface(egldisplay, eglsurface);
}


MirEglApp::~MirEglApp()
{
    eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(egldisplay, eglctx);
    eglTerminate(egldisplay);
    mir_connection_release(connection);
}
