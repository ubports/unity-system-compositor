/*
 * Copyright © 2013 Canonical Ltd.
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

#include "eglapp.h"
#include "mir_toolkit/mir_client_library.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <memory>



float mir_eglapp_background_opacity = 1.0f;

static const char appname[] = "eglspinner";

static volatile sig_atomic_t running = 0;


namespace
{
MirDisplayOutput const* find_first_active_output(MirDisplayConfiguration const* conf)
{
    const MirDisplayOutput *output = NULL;
    int d;

    for (d = 0; d < (int)conf->num_outputs; d++)
    {
        const MirDisplayOutput *out = conf->outputs + d;

        if (out->used &&
            out->connected &&
            out->num_modes &&
            out->current_mode < out->num_modes)
        {
            output = out;
            break;
        }
    }

    return output;
}

void update_dimensions(MirSurfaceParameters& surfaceparm, MirConnection* const connection, unsigned int* width, unsigned int* height)
{
    /* eglapps are interested in the screen size, so
    use mir_connection_create_display_config */
    MirDisplayConfiguration* display_config =
        mir_connection_create_display_config(connection);

    const MirDisplayOutput *output = find_first_active_output(display_config);

    if (!output)
        throw std::runtime_error("No active outputs found.");

    const MirDisplayMode *mode = &output->modes[output->current_mode];

    unsigned int format[mir_pixel_formats];
    unsigned int nformats;

    mir_connection_get_available_surface_formats(
        connection,
        (MirPixelFormat*) format,
        mir_pixel_formats,
        &nformats);

    surfaceparm.pixel_format = (MirPixelFormat) format[0];

    printf("Current active output is %dx%d %+d%+d\n",
           mode->horizontal_resolution, mode->vertical_resolution,
           output->position_x, output->position_y);

    surfaceparm.width = *width > 0 ? *width : mode->horizontal_resolution;
    surfaceparm.height = *height > 0 ? *height : mode->vertical_resolution;

    mir_display_config_destroy(display_config);

    printf("Server supports %d of %d surface pixel formats. Using format: %d\n",
           nformats, mir_pixel_formats, surfaceparm.pixel_format);
}

std::shared_ptr<MirEglApp> mir_egl_app;
std::shared_ptr<MirEglSurface> mir_egl_surface;
}

void mir_eglapp_shutdown(void)
{
    mir_egl_surface.reset();
    mir_egl_app.reset();
}

static void shutdown(int signum)
{
    if (running)
    {
        running = 0;
        printf("Signal %d received. Good night.\n", signum);
    }
}

extern "C" bool mir_eglapp_running(void)
{
    return running;
}

class MirEglSurface
{
public:
    MirEglSurface(std::shared_ptr<MirEglApp> const& mir_egl_app, MirSurfaceParameters const& surfaceparm);

    ~MirEglSurface();
    MirSurface* mir_surface() const { return surface; }

    void egl_make_current() const;

    void swap_buffers();

private:
    std::shared_ptr<MirEglApp> const mir_egl_app;
    MirSurface* const surface;
    EGLSurface const eglsurface;
};

void mir_eglapp_swap_buffers(void)
{
    mir_egl_surface->swap_buffers();
}

bool mir_eglapp_init(int argc, char *argv[],
                                unsigned int *width, unsigned int *height)
{
    MirSurfaceParameters surfaceparm =
        {
            "eglappsurface",
            256, 256,
            mir_pixel_format_xbgr_8888,
            mir_buffer_usage_hardware,
            mir_display_output_id_invalid
        };

    EGLint swapinterval = 1;
    char *mir_socket = NULL;

    if (argc > 1)
    {
        int i;
        for (i = 1; i < argc; i++)
        {
            bool help = 0;
            const char *arg = argv[i];

            if (arg[0] == '-')
            {
                switch (arg[1])
                {
                case 'b':
                    {
                        float alpha = 1.0f;
                        arg += 2;
                        if (!arg[0] && i < argc-1)
                        {
                            i++;
                            arg = argv[i];
                        }
                        if (sscanf(arg, "%f", &alpha) == 1)
                        {
                            mir_eglapp_background_opacity = alpha;
                        }
                        else
                        {
                            printf("Invalid opacity value: %s\n", arg);
                            help = 1;
                        }
                    }
                    break;
                case 'n':
                    swapinterval = 0;
                    break;
                case 'o':
                    {
                        unsigned int output_id = 0;
                        arg += 2;
                        if (!arg[0] && i < argc-1)
                        {
                            i++;
                            arg = argv[i];
                        }
                        if (sscanf(arg, "%u", &output_id) == 1)
                        {
                            surfaceparm.output_id = output_id;
                        }
                        else
                        {
                            printf("Invalid output ID: %s\n", arg);
                            help = 1;
                        }
                    }
                    break;
                case 'f':
                    *width = 0;
                    *height = 0;
                    break;
                case 's':
                    {
                        unsigned int w, h;
                        arg += 2;
                        if (!arg[0] && i < argc-1)
                        {
                            i++;
                            arg = argv[i];
                        }
                        if (sscanf(arg, "%ux%u", &w, &h) == 2)
                        {
                            *width = w;
                            *height = h;
                        }
                        else
                        {
                            printf("Invalid size: %s\n", arg);
                            help = 1;
                        }
                    }
                    break;
                case 'm':
                    mir_socket = argv[++i];
                    break;
                case 'q':
                    {
                        FILE *unused = freopen("/dev/null", "a", stdout);
                        (void)unused;
                        break;
                    }
                case 'h':
                default:
                    help = 1;
                    break;
                }
            }
            else
            {
                help = 1;
            }

            if (help)
            {
                printf("Usage: %s [<options>]\n"
                       "  -b               Background opacity (0.0 - 1.0)\n"
                       "  -h               Show this help text\n"
                       "  -f               Force full screen\n"
                       "  -o ID            Force placement on output monitor ID\n"
                       "  -n               Don't sync to vblank\n"
                       "  -m socket        Mir server socket\n"
                       "  -s WIDTHxHEIGHT  Force surface size\n"
                       "  -q               Quiet mode (no messages output)\n"
                       , argv[0]);
                return 0;
            }
        }
    }

    MirConnection* const connection{mir_connect_sync(mir_socket, appname)};
    if (!mir_connection_is_valid(connection))
        throw std::runtime_error("Can't get connection");

    update_dimensions(surfaceparm, connection, width, height);

    mir_egl_app = make_mir_eglapp(connection, surfaceparm.pixel_format, swapinterval);

    mir_egl_surface = std::make_shared<MirEglSurface>(mir_egl_app, surfaceparm);

    mir_egl_surface->egl_make_current();

    signal(SIGINT, shutdown);
    signal(SIGTERM, shutdown);

    *width = surfaceparm.width;
    *height = surfaceparm.height;

    running = 1;

    return 1;
}


class MirEglApp
{
public:
    MirEglApp(MirConnection* const connection, MirPixelFormat pixel_format, EGLint swapinterval);

    EGLSurface create_surface(MirSurface* surface);

    void release_current();

    void make_current(EGLSurface eglsurface) const;

    void swap_buffers(EGLSurface eglsurface) const;

    ~MirEglApp();
    MirConnection* const connection;
private:
    EGLDisplay egldisplay;
    EGLContext eglctx;
    EGLConfig eglconfig;
    EGLint neglconfigs;
};

std::shared_ptr<MirEglApp> make_mir_eglapp(MirConnection* const connection, MirPixelFormat const& pixel_format, EGLint swapinterval)
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
    mir_egl_app->release_current();
    mir_surface_release_sync(surface);
}

void MirEglSurface::egl_make_current() const
{
    mir_egl_app->make_current(eglsurface);
}

void MirEglSurface::swap_buffers()
{
    mir_egl_app->swap_buffers(eglsurface);
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

    if (!eglInitialize(egldisplay, NULL, NULL))
        throw std::runtime_error("Can't eglInitialize");

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

    eglSwapInterval(egldisplay, swapinterval);
}

EGLSurface MirEglApp::create_surface(MirSurface* surface)
{
    auto const eglsurface = eglCreateWindowSurface(
        egldisplay,
        eglconfig,
        (EGLNativeWindowType)mir_buffer_stream_get_egl_native_window(mir_surface_get_buffer_stream(surface)), NULL);

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

    if (!running)
        return;

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

MirEglApp::~MirEglApp()
{
    eglTerminate(egldisplay);
    mir_connection_release(connection);
}

