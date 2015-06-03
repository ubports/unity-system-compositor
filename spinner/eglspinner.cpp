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
 *
 * Authors: Daniel van Vugt <daniel.van.vugt@canonical.com>
 *          Mirco Müller <mirco.mueller@canonical.com>
 */

#include "eglapp.h"
#include "miregl.h"
#include <assert.h>
#include <cairo.h>
#include <glib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#if HAVE_PROPS
#include <hybris/properties/properties.h>
#endif
#include <signal.h>

// this is needed for get_gu() to obtain the grid-unit value
#define MAX_LENGTH       256
#define VALUE_KEY        "GRID_UNIT_PX"
#define VALUE_KEY_LENGTH 12
#define PROP_KEY         "ro.product.device"
#define DEFAULT_FILE     "/etc/ubuntu-touch-session.d/android.conf"
#define FILE_BASE        "/etc/ubuntu-touch-session.d/"
#define FILE_EXTENSION   ".conf"

int get_gu ()
{
    int   gu           = 10; // use 10 as a default value
    FILE* handle       = NULL;
    int   i            = 0;
    int   j            = 0;
    int   len          = 0;
    char  line[MAX_LENGTH];
    char  filename[MAX_LENGTH];

    // get name of file to read from
    bzero ((void*) filename, MAX_LENGTH);
    strcpy (filename, FILE_BASE);

    struct stat buf;   
    if (stat(DEFAULT_FILE, &buf) == 0)
    {
        strcpy (filename, DEFAULT_FILE);
    }
    else
    {        
#ifdef HAVE_PROPS
        char const* defaultValue = "";
        char  value[PROP_VALUE_MAX];
        property_get (PROP_KEY, value, defaultValue);
        strcat (filename, value);
#endif
        strcat (filename, FILE_EXTENSION);
    }

    // try to open it
    handle = fopen ((const char*) filename, "r");
    if (!handle)
        return gu;

    // read one line at a time
    while (fgets (line, MAX_LENGTH, handle))
    {
        // strip line of whitespaces
        i = 0;
        j = 0;
        len = (int) strlen (line);
        while (i != len)
        {
            if (line[i] != ' ' && line[i] != '\t')
                line[j++] = line[i];
            i++;
        }
        line[j] = 0;

        // parse the line for GU-value
        if (!strncmp (line, VALUE_KEY, VALUE_KEY_LENGTH))
            sscanf (line, VALUE_KEY"=%d", &gu);
    }

    // clean up
    fclose (handle);

    return gu;
}

static GLuint load_shader(const char *src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    if (shader)
    {
        GLint compiled;
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLchar log[1024];
            glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
            log[sizeof log - 1] = '\0';
            printf("load_shader compile failed: %s\n", log);
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

// Colours from http://design.ubuntu.com/brand/colour-palette
//#define MID_AUBERGINE   0.368627451f, 0.152941176f, 0.31372549f
//#define ORANGE          0.866666667f, 0.282352941f, 0.141414141f
//#define WARM_GREY       0.682352941f, 0.654901961f, 0.623529412f
//#define COOL_GREY       0.2f,         0.2f,         0.2f
//#define LIGHT_AUBERGINE 0.466666667f, 0.297297297f, 0.435294118f
//#define DARK_AUBERGINE  0.17254902f,  0.0f,         0.117647059f
#define BLACK           0.0f,         0.0f,         0.0f
//#define WHITE           1.0f,         1.0f,         1.0f

cairo_surface_t* pngToSurface (const char* filename)
{
    if (access(filename, F_OK & R_OK) != 0)
        throw std::runtime_error("Failed to load png: " + std::string(filename) + "\n");

    // create surface from PNG
    cairo_surface_t* surface = NULL;
    surface = cairo_image_surface_create_from_png (filename);
    if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
        throw std::runtime_error("Failed to load png: " + std::string(filename) + "\n");

    return surface;
}

void uploadTexture (GLuint id, const char* filename)
{
    if (!id || !filename)
        return;

    cairo_surface_t* surface = pngToSurface (filename);

    glBindTexture(GL_TEXTURE_2D, id);

    if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32 ? GL_RGBA : GL_RGB,
                     cairo_image_surface_get_width (surface),
                     cairo_image_surface_get_height (surface),
                     0,
                     cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32 ? GL_RGBA : GL_RGB,
                     GL_UNSIGNED_BYTE,
                     cairo_image_surface_get_data (surface));
        cairo_surface_destroy (surface);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint createShaderProgram(const char* vertexShaderSrc, const char* fragmentShaderSrc)
{
    if (!vertexShaderSrc || !fragmentShaderSrc)
        return 0;

    GLuint vShaderId = 0;
    vShaderId = load_shader(vertexShaderSrc, GL_VERTEX_SHADER);
    assert(vShaderId);

    GLuint fShaderId = 0;
    fShaderId = load_shader(fragmentShaderSrc, GL_FRAGMENT_SHADER);
    assert(fShaderId);

    GLuint progId = 0;
    progId = glCreateProgram();
    assert(progId);
    glAttachShader(progId, vShaderId);
    glAttachShader(progId, fShaderId);
    glLinkProgram(progId);

    GLint linked = 0;
    glGetProgramiv(progId, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLchar log[1024];
        glGetProgramInfoLog(progId, sizeof log - 1, NULL, log);
        log[sizeof log - 1] = '\0';
        printf("Link failed: %s\n", log);
        return 0;
    }

    return progId;
}

typedef struct _AnimationValues
{
    double lastTimeStamp;
    GLfloat angle;
    GLfloat fadeBackground;
    GLfloat fadeLogo;
    GLfloat fadeGlow;
} AnimationValues;

void
updateAnimation (GTimer* timer, AnimationValues* anim)
{
    if (!timer || !anim)
        return;

    //1.) 0.0   - 0.6:   logo fades in fully
    //2.) 0.0   - 6.0:   logo does one full spin 360°
    //3.) 6.0   - 6.833: glow fades in fully, black-background fades out to 50%
    //4.) 6.833 - 7.666: glow fades out fully, black-background fades out to 0% 
    //5.) 7.666 - 8.266: logo fades out fully
    //8.266..:       now spinner can be closed as all its elements are faded out

    double elapsed = g_timer_elapsed (timer, NULL);
    double dt = elapsed - anim->lastTimeStamp;
    anim->lastTimeStamp = elapsed;

    // step 1.)
    if (elapsed < 0.6f)
        anim->fadeLogo += 1.6f * dt;

    // step 2.)
    anim->angle -= (0.017453292519943f * 360.0f / 6.0f) * dt;

    // step 3.) glow
    if (elapsed > 6.0f && elapsed < 6.833f)
        anim->fadeGlow += 1.2f * dt;

    // Ignore the following three until we can synchronize with greeter

    // step 3.) background
    //if (elapsed > 6.0f && elapsed < 6.833f)
    //    anim->fadeBackground -= 0.6f * dt;

    // step 4.) background
    //if (elapsed > 7.0f)
    //    anim->fadeBackground -= 0.6f * dt;

    // step 5.)
    //if (elapsed > 6.833f)
    //    anim->fadeLogo -= 1.6f * dt;
}

namespace
{
const char vShaderSrcSpinner[] =
    "attribute vec4 vPosition;                       \n"
    "attribute vec2 aTexCoords;                      \n"
    "uniform float theta;                            \n"
    "varying vec2 vTexCoords;                        \n"
    "void main()                                     \n"
    "{                                               \n"
    "    float c = cos(theta);                       \n"
    "    float s = sin(theta);                       \n"
    "    mat2 m;                                     \n"
    "    m[0] = vec2(c, s);                          \n"
    "    m[1] = vec2(-s, c);                         \n"
    "    vTexCoords = m * aTexCoords + vec2 (0.5, 0.5); \n"
    "    gl_Position = vec4(vPosition.xy, -1.0, 1.0); \n"
    "}                                               \n";

const char fShaderSrcGlow[] =
    "precision mediump float;                             \n"
    "varying vec2 vTexCoords;                             \n"
    "uniform sampler2D uSampler;                          \n"
    "uniform float uFadeGlow;                             \n"
    "void main()                                          \n"
    "{                                                    \n"
    "    // swizzle because texture was created with cairo\n"
    "    vec4 col = texture2D(uSampler, vTexCoords).bgra; \n"
    "    float r = col.r * uFadeGlow;                     \n"
    "    float g = col.g * uFadeGlow;                     \n"
    "    float b = col.b * uFadeGlow;                     \n"
    "    float a = col.a * uFadeGlow;                     \n"
    "    gl_FragColor = vec4(r, g, b, a);                 \n"
    "}                                                    \n";

const char fShaderSrcLogo[] =
    "precision mediump float;                             \n"
    "varying vec2 vTexCoords;                             \n"
    "uniform sampler2D uSampler;                          \n"
    "uniform float uFadeLogo;                             \n"
    "void main()                                          \n"
    "{                                                    \n"
    "    // swizzle because texture was created with cairo\n"
    "    vec4 col = texture2D(uSampler, vTexCoords).bgra; \n"
    "    float r = col.r * uFadeLogo;                     \n"
    "    float g = col.g * uFadeLogo;                     \n"
    "    float b = col.b * uFadeLogo;                     \n"
    "    float a = col.a * uFadeLogo;                     \n"
    "    gl_FragColor = vec4(r, g, b, a);                 \n"
    "}                                                    \n";

static volatile sig_atomic_t running = 0;

static void shutdown(int signum)
{
    if (running)
    {
        running = 0;
        printf("Signal %d received. Good night.\n", signum);
    }
}

bool mir_eglapp_running()
{
    return running;
}
}

int main(int argc, char *argv[])
try
{
    GLuint prog[2];
    GLuint texture[2];
    GLint vpos[2];
    GLint theta;
    GLint fadeGlow;
    GLint fadeLogo;
    GLint aTexCoords[2];
    GLint sampler[2];

    auto const surfaces = mir_eglapp_init(argc, argv);

    if (!surfaces.size())
    {
        printf("No surfaces created\n");
        return EXIT_SUCCESS;
    }

    running = 1;
    signal(SIGINT, shutdown);
    signal(SIGTERM, shutdown);

    double pixelSize = get_gu() * 11.18;
    const GLfloat texCoordsSpinner[] =
    {
        -0.5f, 0.5f,
        -0.5f, -0.5f,
        0.5f, 0.5f,
        0.5f, -0.5f,
    };

    prog[0] = createShaderProgram(vShaderSrcSpinner, fShaderSrcGlow);
    prog[1] = createShaderProgram(vShaderSrcSpinner, fShaderSrcLogo);

    // setup proper GL-blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // get locations of shader-attributes/uniforms
    vpos[0] = glGetAttribLocation(prog[0], "vPosition");
    aTexCoords[0] = glGetAttribLocation(prog[0], "aTexCoords");
    theta = glGetUniformLocation(prog[0], "theta");
    sampler[0] = glGetUniformLocation(prog[0], "uSampler");
    fadeGlow = glGetUniformLocation(prog[0], "uFadeGlow");
    vpos[1] = glGetAttribLocation(prog[1], "vPosition");
    aTexCoords[1] = glGetAttribLocation(prog[1], "aTexCoords");
    sampler[1] = glGetUniformLocation(prog[1], "uSampler");
    fadeLogo = glGetUniformLocation(prog[1], "uFadeLogo");

    // create and upload spinner-artwork
    glGenTextures(2, texture);
    uploadTexture(texture[0], PKGDATADIR "/spinner-glow.png");
    uploadTexture(texture[1], PKGDATADIR "/spinner-logo.png");

    // bunch of shader-attributes to enable
    glVertexAttribPointer(aTexCoords[0], 2, GL_FLOAT, GL_FALSE, 0, texCoordsSpinner);
    glVertexAttribPointer(aTexCoords[1], 2, GL_FLOAT, GL_FALSE, 0, texCoordsSpinner);
    glEnableVertexAttribArray(vpos[0]);
    glEnableVertexAttribArray(vpos[1]);
    glEnableVertexAttribArray(aTexCoords[0]);
    glEnableVertexAttribArray(aTexCoords[1]);
    glActiveTexture(GL_TEXTURE0);

    AnimationValues anim = {0.0, 0.0, 1.0, 0.0, 0.0};
    GTimer* timer = g_timer_new();

    while (mir_eglapp_running())
    {
        for (auto const& surface : surfaces)
            surface->paint([&](unsigned int width, unsigned int height)
            {
                GLfloat halfRealWidth = ((2.0 / width) * pixelSize) / 2.0;
                GLfloat halfRealHeight = ((2.0 / height) * pixelSize) / 2.0;

                const GLfloat vertices[] =
                    {
                        halfRealWidth,  halfRealHeight,
                        halfRealWidth, -halfRealHeight,
                        -halfRealWidth, halfRealHeight,
                        -halfRealWidth,-halfRealHeight,
                    };

                glVertexAttribPointer(vpos[0], 2, GL_FLOAT, GL_FALSE, 0, vertices);
                glVertexAttribPointer(vpos[1], 2, GL_FLOAT, GL_FALSE, 0, vertices);

                glViewport(0, 0, width, height);

                glClearColor(BLACK, anim.fadeBackground);
                glClear(GL_COLOR_BUFFER_BIT);

                // draw glow
                glUseProgram(prog[0]);
                glBindTexture(GL_TEXTURE_2D, texture[0]);
                glUniform1i(sampler[0], 0);
                glUniform1f(theta, anim.angle);
                glUniform1f(fadeGlow, anim.fadeGlow);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw logo
                glUseProgram(prog[1]);
                glBindTexture(GL_TEXTURE_2D, texture[1]);
                glUniform1i(sampler[1], 0);
                glUniform1f(theta, anim.angle);
                glUniform1f(fadeLogo, anim.fadeLogo);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            });

        // update animation variable
        updateAnimation(timer, &anim);
    }

    glDeleteTextures(2, texture);
    g_timer_destroy (timer);

    return EXIT_SUCCESS;
}
catch (std::exception const& x)
{
    printf("%s\n", x.what());
    return EXIT_FAILURE;
}