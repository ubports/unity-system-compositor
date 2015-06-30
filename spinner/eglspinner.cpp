/*
 * Copyright © 2013-2015 Canonical Ltd.
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
 *          Alan Griffiths <alan@octopull.co.uk>
 *          Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "eglapp.h"
#include "miregl.h"
#include <assert.h>
#include <glib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <sys/stat.h>
#if HAVE_PROPS
#include <hybris/properties/properties.h>
#endif
#include <signal.h>

#include "wallpaper.h"
#include "logo.h"
#include "white_dot.h"
#include "orange_dot.h"

// this is needed for get_gu() to obtain the grid-unit value
#define MAX_LENGTH       256
#define VALUE_KEY        "GRID_UNIT_PX"
#define VALUE_KEY_LENGTH 12
#define PROP_KEY         "ro.product.device"
#define DEFAULT_FILE     "/etc/ubuntu-touch-session.d/android.conf"
#define FILE_BASE        "/etc/ubuntu-touch-session.d/"
#define FILE_EXTENSION   ".conf"

enum TextureIds {
    WALLPAPER = 0,
    LOGO,
    WHITE_DOT,
    ORANGE_DOT,
    MAX_TEXTURES
};

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

template <typename Image>
void uploadTexture (GLuint id, Image& image)
{
    glBindTexture(GL_TEXTURE_2D, id);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 image.width,
                 image.height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 image.pixel_data);
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
    GLfloat fadeBackground;
    int dot_mask;
} AnimationValues;

void ortho(GLfloat* mat,
           GLfloat left,
           GLfloat right,
           GLfloat bottom,
           GLfloat top,
           GLfloat near,
           GLfloat far)
{
    if (right == left ||
        top == bottom ||
        far == near ||
        mat == NULL)
    {
        return;
    }

    mat[0]  = 2.0f / (right - left);
    mat[1]  = 0.0f;
    mat[2]  = 0.0f;
    mat[3]  = 0.0f;

    mat[4]  = 0.0f;
    mat[5]  = 2.0f / (top - bottom);
    mat[6]  = 0.0f;
    mat[7]  = 0.0f;

    mat[8]  = 0.0f;
    mat[9]  = 0.0f;
    mat[10] = -2.0f / (far - near);
    mat[11] = 0.0f;

    mat[12] = -(right + left) / (right - left);
    mat[13] = -(top + bottom) / (top - bottom);
    mat[14] = -(far + near) / (far - near);
    mat[15] = 1.0f;
}

GLfloat gu2px(GLfloat gu) {
    static GLfloat pixelsPerGU = get_gu();

    return gu * pixelsPerGU;
}

void
updateAnimation (GTimer* timer, AnimationValues* anim)
{
    if (!timer || !anim)
        return;

    static const int sequence[] = {0, 1, 3, 7, 15, 31};
    static int counter = 0;
    static double second = 0.0f;

    double elapsed = g_timer_elapsed (timer, NULL);

    if (second >= 1.0f) {
        second = 0.0f;
        counter++;
    } else {
        second += elapsed - anim->lastTimeStamp;
    }
    anim->dot_mask = sequence[counter%6];

    anim->lastTimeStamp = elapsed;
}

namespace
{
const char vShaderSrcPlain[] =
    "attribute vec4 aPosition;                                               \n"
    "attribute vec2 aTexCoords;                                              \n"
    "uniform vec2 uOffset;                                                   \n"
    "varying vec2 vTexCoords;                                                \n"
    "uniform mat4 uProjMat;                                                  \n"
    "void main()                                                             \n"
    "{                                                                       \n"
    "    vTexCoords = aTexCoords + vec2 (0.5, 0.5);                          \n"
    "    gl_Position = uProjMat * vec4(aPosition.xy + uOffset.xy, 0.0, 1.0);\n"
    "}                                                                       \n";

const char fShaderSrcPlain[] =
    "precision mediump float;                           \n"
    "varying vec2 vTexCoords;                           \n"
    "uniform sampler2D uSampler;                        \n"
    "void main()                                        \n"
    "{                                                  \n"
    "    gl_FragColor = texture2D(uSampler, vTexCoords);\n"
    "}                                                  \n";

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
    GLuint prog[3];
    GLuint texture[MAX_TEXTURES];
    GLint vpos[MAX_TEXTURES];
    GLint aTexCoords[MAX_TEXTURES];
    GLint sampler[MAX_TEXTURES];
    GLint offset[MAX_TEXTURES];
    GLint projMat[MAX_TEXTURES];

    auto const surfaces = mir_eglapp_init(argc, argv);

    if (!surfaces.size())
    {
        printf("No surfaces created\n");
        return EXIT_SUCCESS;
    }

    running = 1;
    signal(SIGINT, shutdown);
    signal(SIGTERM, shutdown);

    const GLfloat texCoords[] =
    {
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
        -0.5f,  0.5f,
    };

    prog[WALLPAPER] = createShaderProgram(vShaderSrcPlain, fShaderSrcPlain);
    prog[LOGO] = createShaderProgram(vShaderSrcPlain, fShaderSrcPlain);
    prog[WHITE_DOT] = createShaderProgram(vShaderSrcPlain, fShaderSrcPlain);

    // setup proper GL-blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // get locations of shader-attributes/uniforms
    vpos[WALLPAPER]         = glGetAttribLocation(prog[WALLPAPER],  "aPosition");
    aTexCoords[WALLPAPER]   = glGetAttribLocation(prog[WALLPAPER],  "aTexCoords");
    sampler[WALLPAPER]      = glGetUniformLocation(prog[WALLPAPER], "uSampler");
    offset[WALLPAPER]       = glGetUniformLocation(prog[WALLPAPER], "uOffset");
    projMat[WALLPAPER]      = glGetUniformLocation(prog[WALLPAPER], "uProjMat");

    vpos[LOGO]         = glGetAttribLocation(prog[LOGO],  "aPosition");
    aTexCoords[LOGO]   = glGetAttribLocation(prog[LOGO],  "aTexCoords");
    sampler[LOGO]      = glGetUniformLocation(prog[LOGO], "uSampler");
    offset[LOGO]       = glGetUniformLocation(prog[LOGO], "uOffset");
    projMat[LOGO]      = glGetUniformLocation(prog[LOGO], "uProjMat");

    vpos[WHITE_DOT]         = glGetAttribLocation(prog[WHITE_DOT],  "aPosition");
    aTexCoords[WHITE_DOT]   = glGetAttribLocation(prog[WHITE_DOT],  "aTexCoords");
    sampler[WHITE_DOT]      = glGetUniformLocation(prog[WHITE_DOT], "uSampler");
    offset[WHITE_DOT]       = glGetUniformLocation(prog[WHITE_DOT], "uOffset");
    projMat[WHITE_DOT]      = glGetUniformLocation(prog[WHITE_DOT], "uProjMat");

    // create and upload spinner-artwork
    // note that the embedded image data has pre-multiplied alpha
    glGenTextures(MAX_TEXTURES, texture);
    uploadTexture(texture[WALLPAPER], wallpaper);
    uploadTexture(texture[LOGO], logo);
    uploadTexture(texture[WHITE_DOT], white_dot);
    uploadTexture(texture[ORANGE_DOT], orange_dot);

    // bunch of shader-attributes to enable
    glVertexAttribPointer(aTexCoords[WALLPAPER], 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(vpos[WALLPAPER]);
    glEnableVertexAttribArray(aTexCoords[WALLPAPER]);
    glVertexAttribPointer(aTexCoords[LOGO], 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(vpos[LOGO]);
    glEnableVertexAttribArray(aTexCoords[LOGO]);
    glVertexAttribPointer(aTexCoords[WHITE_DOT], 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(vpos[WHITE_DOT]);
    glEnableVertexAttribArray(aTexCoords[WHITE_DOT]);
    glActiveTexture(GL_TEXTURE0);

    AnimationValues anim = {0.0, 0.0, 0};
    GTimer* timer = g_timer_new();

    while (mir_eglapp_running())
    {
        for (auto const& surface : surfaces)
            surface->paint([&](unsigned int width, unsigned int height)
            {
                GLfloat logoWidth = gu2px (14.5f);
                GLfloat logoHeight = gu2px (3.0f);
                GLfloat dotSize = gu2px (0.5f);
                GLfloat dotXGap = gu2px (5.0f);
                GLfloat dotYGap = gu2px (2.0f);

                const GLfloat fullscreen[] = {
                    (GLfloat) width, 0.0f,
                    (GLfloat) width, (GLfloat) height,
                    0.0f,            0.0f,
                    0.0f,            (GLfloat) height
                };

                const GLfloat logo[] = {
                    logoWidth, 0.0f,
                    logoWidth, logoHeight,
                    0.0f,      0.0f,
                    0.0f,      logoHeight
                };

                const GLfloat dot[] = {
                    dotSize, 0.0f,
                    dotSize, dotSize,
                    0.0f,    0.0f,
                    0.0f,    dotSize
                };

                GLfloat projMatrix[16];
                ortho(&projMatrix[0], 0.0f, (GLfloat) width, (GLfloat) height, 0.0f, -1.0f, 1.0f);

                glViewport(0, 0, width, height);

                glClearColor(BLACK, anim.fadeBackground);
                glClear(GL_COLOR_BUFFER_BIT);

                // draw wallpaper backdrop
                glVertexAttribPointer(vpos[WALLPAPER], 2, GL_FLOAT, GL_FALSE, 0, fullscreen);
                glUseProgram(prog[WALLPAPER]);
                glBindTexture(GL_TEXTURE_2D, texture[WALLPAPER]);
                glUniform1i(sampler[WALLPAPER], 0);
                glUniform2f(offset[WALLPAPER], 0.0f, 0.0f);
                glUniformMatrix4fv(projMat[WALLPAPER], 1, GL_FALSE,  projMatrix);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw logo
                glVertexAttribPointer(vpos[LOGO], 2, GL_FLOAT, GL_FALSE, 0, logo);
                glUseProgram(prog[LOGO]);
                glBindTexture(GL_TEXTURE_2D, texture[LOGO]);
                glUniform1i(sampler[LOGO], 0);
                glUniform2f(offset[LOGO], width/2.0f - logoWidth / 2.0f, height / 2.0f - logoHeight / 2.0f);
                glUniformMatrix4fv(projMat[LOGO], 1, GL_FALSE,  projMatrix);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw white dots
                glVertexAttribPointer(vpos[WHITE_DOT], 2, GL_FLOAT, GL_FALSE, 0, dot);
                glUseProgram(prog[WHITE_DOT]);
                glUniform1i(sampler[WHITE_DOT], 0);
                glUniformMatrix4fv(projMat[WHITE_DOT], 1, GL_FALSE,  projMatrix);
                for (int i = -2; i < 3; i++) {
                    glBindTexture(GL_TEXTURE_2D, texture[anim.dot_mask >> (i + 2) ? ORANGE_DOT : WHITE_DOT]);
                    glUniform2f(offset[WHITE_DOT], width/2.0f + i * dotXGap, height / 2.0f + logoHeight / 2.0f + dotYGap);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);                    
                }
            });

        // update animation variable
        updateAnimation(timer, &anim);
    }

    glDeleteTextures(MAX_TEXTURES, texture);
    g_timer_destroy (timer);

    return EXIT_SUCCESS;
}
catch (std::exception const& x)
{
    printf("%s\n", x.what());
    return EXIT_FAILURE;
}
