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

#define CHECK(_cond, _err) \
    if (!(_cond)) \
    { \
        printf("%s\n", (_err)); \
        return 0; \
    }


namespace
{
MirSurfaceParameters surfaceparm =
    {
        "eglappsurface",
        256, 256,
        mir_pixel_format_xbgr_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };

static const MirDisplayOutput *find_active_output(
    const MirDisplayConfiguration *conf)
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

struct MirEglApp
{
    MirEglApp(char const* socket, char const* name, unsigned int* width, unsigned int* height) :
        connection{mir_connect_sync(socket, appname)}
    {
        if (!mir_connection_is_valid(connection))
            throw std::runtime_error("Can't get connection");

        /* eglapps are interested in the screen size, so
           use mir_connection_create_display_config */
        MirDisplayConfiguration* display_config =
            mir_connection_create_display_config(connection);

        const MirDisplayOutput *output = find_active_output(display_config);

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

        unsigned int bpp = 8 * MIR_BYTES_PER_PIXEL(surfaceparm.pixel_format);

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
    }

    ~MirEglApp()
    {
        eglTerminate(egldisplay);
        mir_connection_release(connection);
    }

    MirConnection* const connection;
    EGLDisplay egldisplay;
    EGLConfig eglconfig;
    EGLContext eglctx;
private:
    EGLint neglconfigs;
};

std::shared_ptr<MirEglApp> mir_egl_app;

inline MirSurface* create_surface(std::shared_ptr<MirEglApp> const& mir_egl_app)
{
    auto const spec = mir_connection_create_spec_for_normal_surface(
        mir_egl_app->connection,
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

class MirEglSurface
{
public:
    MirEglSurface(std::shared_ptr<MirEglApp> const& mir_egl_app, unsigned int */*width*/, unsigned int */*height*/) :
        surface{create_surface(mir_egl_app)},
        eglsurface{eglCreateWindowSurface(
            mir_egl_app->egldisplay, mir_egl_app->eglconfig,
            (EGLNativeWindowType)mir_buffer_stream_get_egl_native_window(mir_surface_get_buffer_stream(surface)), NULL)}
    {
        if (eglsurface == EGL_NO_SURFACE)
            throw std::runtime_error("eglCreateWindowSurface failed");
    }

    ~MirEglSurface()
    {
        eglMakeCurrent(mir_egl_app->egldisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        mir_surface_release_sync(surface);
    }

    MirSurface* mir_surface() const { return surface; }

    void egl_make_current() const
    {
        if (!eglMakeCurrent(mir_egl_app->egldisplay, eglsurface, eglsurface, mir_egl_app->eglctx))
            throw std::runtime_error("Can't eglMakeCurrent");
    }

    void swap_buffers()
    {
        EGLint width;
        EGLint height;

        if (!running)
            return;

        eglSwapBuffers(mir_egl_app->egldisplay, eglsurface);

        /*
         * Querying the surface (actually the current buffer) dimensions here is
         * the only truly safe way to be sure that the dimensions we think we
         * have are those of the buffer being rendered to. But this should be
         * improved in future; https://bugs.launchpad.net/mir/+bug/1194384
         */
        if (eglQuerySurface(mir_egl_app->egldisplay, eglsurface, EGL_WIDTH, &width) &&
            eglQuerySurface(mir_egl_app->egldisplay, eglsurface, EGL_HEIGHT, &height))
        {
            glViewport(0, 0, width, height);
        }
    }

private:
    MirSurface* const surface;
    EGLSurface const eglsurface;
};

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

void mir_eglapp_swap_buffers(void)
{
    mir_egl_surface->swap_buffers();
}

bool mir_eglapp_init(int argc, char *argv[],
                                unsigned int *width, unsigned int *height)
{
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

    mir_egl_app = std::make_shared<MirEglApp>(mir_socket, appname, width, height);

    mir_egl_surface = std::make_shared<MirEglSurface>(mir_egl_app, width, height);

    mir_egl_surface->egl_make_current();

    signal(SIGINT, shutdown);
    signal(SIGTERM, shutdown);

    *width = surfaceparm.width;
    *height = surfaceparm.height;

    eglSwapInterval(mir_egl_app->egldisplay, swapinterval);

    running = 1;

    return 1;
}

