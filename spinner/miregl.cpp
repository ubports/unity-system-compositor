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

#include <cstring>

#include <GLES2/gl2.h>

class MirEglApp
{
public:
    MirEglApp(MirConnection* const connection, MirPixelFormat pixel_format);

    EGLSurface create_surface(MirSurface* surface);

    void release_current();

    void make_current(EGLSurface eglsurface) const;

    void swap_buffers(EGLSurface eglsurface) const;

    void destroy_surface(EGLSurface eglsurface) const;

    void get_surface_size(EGLSurface eglsurface, int* width, int* height) const;

    void set_swap_interval(EGLSurface eglsurface, int interval) const;

    bool supports_surfaceless_context();

    ~MirEglApp();

    MirConnection* const connection;
private:
    EGLDisplay egldisplay;
    EGLContext eglctx;
    EGLConfig eglconfig;
    EGLint neglconfigs;
    EGLSurface dummy_surface;
};

std::shared_ptr<MirEglApp> make_mir_eglapp(
    MirConnection* const connection, MirPixelFormat const& pixel_format)
{
    return std::make_shared<MirEglApp>(connection, pixel_format);
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
        throw std::runtime_error(std::string("Can't create a surface ") + mir_surface_get_error_message(surface));

    if (surfaceparm.output_id != mir_display_output_id_invalid)
        mir_surface_set_state(surface, mir_surface_state_fullscreen);

    return surface;
}
}

MirEglSurface::MirEglSurface(std::shared_ptr<MirEglApp> const& mir_egl_app, MirSurfaceParameters const& surfaceparm, int swapinterval) :
    mir_egl_app{mir_egl_app},
    surface{create_surface(mir_egl_app->connection, surfaceparm)},
    eglsurface{mir_egl_app->create_surface(surface)},
    width_{0},
    height_{0}
{
    mir_egl_app->set_swap_interval(eglsurface, swapinterval);
}

MirEglSurface::~MirEglSurface()
{
    mir_egl_app->destroy_surface(eglsurface);
    mir_surface_release_sync(surface);
}

void MirEglSurface::egl_make_current()
{
    mir_egl_app->get_surface_size(eglsurface, &width_, &height_);
    mir_egl_app->make_current(eglsurface);
}

void MirEglSurface::swap_buffers()
{
    mir_egl_app->swap_buffers(eglsurface);
}

unsigned int MirEglSurface::width() const
{
    return width_;
}

unsigned int MirEglSurface::height() const
{
    return height_;
}

MirEglApp::MirEglApp(MirConnection* const connection, MirPixelFormat pixel_format) :
    connection{connection},
    dummy_surface{EGL_NO_SURFACE}
{
    unsigned int bpp = 8*MIR_BYTES_PER_PIXEL(pixel_format);

    EGLint attribs[] =
        {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
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

    if (!supports_surfaceless_context())
    {
        static EGLint const dummy_pbuffer_attribs[] =
        {
             EGL_WIDTH, 1,
             EGL_HEIGHT, 1,
             EGL_NONE
        };
     
        dummy_surface = eglCreatePbufferSurface(egldisplay, eglconfig, dummy_pbuffer_attribs);
        if (dummy_surface == EGL_NO_SURFACE)
            throw std::runtime_error("eglCreatePbufferSurface failed");
    }

    make_current(dummy_surface);
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

void MirEglApp::make_current(EGLSurface eglsurface) const
{
    if (!eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglctx))
        throw std::runtime_error("Can't eglMakeCurrent");
}

void MirEglApp::swap_buffers(EGLSurface eglsurface) const
{
    eglSwapBuffers(egldisplay, eglsurface);
}

void MirEglApp::destroy_surface(EGLSurface eglsurface) const
{
    eglDestroySurface(egldisplay, eglsurface);
}

void MirEglApp::get_surface_size(EGLSurface eglsurface, int* width, int* height) const
{
    eglQuerySurface(egldisplay, eglsurface, EGL_WIDTH, width);
    eglQuerySurface(egldisplay, eglsurface, EGL_HEIGHT, height);
}

void MirEglApp::set_swap_interval(EGLSurface eglsurface, int interval) const
{
    auto const previous_surface = eglGetCurrentSurface(EGL_DRAW);

    make_current(eglsurface);
    eglSwapInterval(egldisplay, interval);

    if (previous_surface != EGL_NO_SURFACE)
        make_current(previous_surface);
}

bool MirEglApp::supports_surfaceless_context()
{
    auto const extensions = eglQueryString(egldisplay, EGL_EXTENSIONS);
    if (!extensions)
        return false;
    return std::strstr(extensions, "EGL_KHR_surfaceless_context") != nullptr;
}

MirEglApp::~MirEglApp()
{
    eglMakeCurrent(egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (dummy_surface != EGL_NO_SURFACE)
        destroy_surface(dummy_surface);
    eglDestroyContext(egldisplay, eglctx);
    eglTerminate(egldisplay);
    mir_connection_release(connection);
}
