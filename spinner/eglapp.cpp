/*
 * Copyright © 2013, 2015 Canonical Ltd.
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

#include "miregl.h"



float mir_eglapp_background_opacity = 1.0f;

static const char appname[] = "eglspinner";


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

void update_surfaceparm(
    MirSurfaceParameters& surfaceparm,
    MirConnection* const connection,
    unsigned int* width,
    unsigned int* height)
{
    /* eglapps are interested in the screen size, so
    use mir_connection_create_display_config */
    MirDisplayConfiguration* display_config =
        mir_connection_create_display_config(connection);

    const MirDisplayOutput *output = find_first_active_output(display_config);

    if (!output)
        throw std::runtime_error("No active outputs found.");

    const MirDisplayMode *mode = &output->modes[output->current_mode];

    printf("Current active output is %dx%d %+d%+d\n",
           mode->horizontal_resolution, mode->vertical_resolution,
           output->position_x, output->position_y);

    surfaceparm.width = *width > 0 ? *width : mode->horizontal_resolution;
    surfaceparm.height = *height > 0 ? *height : mode->vertical_resolution;

    mir_display_config_destroy(display_config);
}

void select_pixel_format(MirSurfaceParameters& surfaceparm, MirConnection* connection)
{
    unsigned int format[mir_pixel_formats];
    unsigned int nformats;

    mir_connection_get_available_surface_formats(
        connection,
        (MirPixelFormat*) format,
        mir_pixel_formats,
        &nformats);

    surfaceparm.pixel_format = (MirPixelFormat) format[0];

    printf("Server supports %d of %d surface pixel formats. Using format: %d\n",
           nformats, mir_pixel_formats, surfaceparm.pixel_format);
}
}

std::vector<std::shared_ptr<MirEglSurface>> mir_eglapp_init(int argc, char *argv[],
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
                return {};
            }
        }
    }

    MirConnection* const connection{mir_connect_sync(mir_socket, appname)};
    if (!mir_connection_is_valid(connection))
        throw std::runtime_error("Can't get connection");

    select_pixel_format(surfaceparm, connection);

    auto const mir_egl_app = make_mir_eglapp(connection, surfaceparm.pixel_format, swapinterval);

    update_surfaceparm(surfaceparm, connection, width, height);

    *width = surfaceparm.width;
    *height = surfaceparm.height;

    auto const mir_egl_surface = std::make_shared<MirEglSurface>(mir_egl_app, surfaceparm);
    return {mir_egl_surface};
}

