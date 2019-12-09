/*
 * Copyright © 2013-2016 Canonical Ltd.
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

#include "eglsurface.h"
#include "mir/eglapp.h"
#include "wayland/eglapp.h"
#include <assert.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <deviceinfo.h>
#include <EGL/egl.h>

#include <signal.h>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>
#include <iostream>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "robot.h"
#include "white_dot.h"
#include "orange_dot.h"

#define WALLPAPER_FILE "/usr/share/backgrounds/warty-final-ubuntu.png"

enum TextureIds {
    WALLPAPER = 0,
    ROBOT,
    WHITE_DOT,
    ORANGE_DOT,
    MAX_TEXTURES
};

enum Backend {
    WAYLAND,
    MIR,
    UNKOWN = -1
};

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

static struct {
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int bytes_per_pixel = 3; /* 3:RGB, 4:RGBA */
    unsigned char *pixel_data = nullptr;
} wallpaper;

template <typename Image>
void uploadTexture (GLuint id, Image& image)
{
    glBindTexture(GL_TEXTURE_2D, id);
    GLint format = GL_RGBA;
    if (image.bytes_per_pixel == 3)
        format = GL_RGB;

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 format,
                 image.width,
                 image.height,
                 0,
                 format,
                 GL_UNSIGNED_BYTE,
                 image.pixel_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
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

    auto deviceinfo = std::make_shared<DeviceInfo>();

    // Load wallpaper
    GError *error = nullptr;
    auto wallpaperPixbuf = gdk_pixbuf_new_from_file(WALLPAPER_FILE, &error);
    if (wallpaperPixbuf) {
        wallpaper.width = gdk_pixbuf_get_width(wallpaperPixbuf);
        wallpaper.height = gdk_pixbuf_get_height(wallpaperPixbuf);
        wallpaper.bytes_per_pixel = gdk_pixbuf_get_has_alpha(wallpaperPixbuf) ? 4 : 3;
        wallpaper.pixel_data = (unsigned char*)gdk_pixbuf_read_pixels(wallpaperPixbuf);
    } else {
        printf("Could not load wallpaper: %s\n", error->message);
    }
    g_clear_error(&error);

    std::vector<std::shared_ptr<EglSurface>> surfaces;
    char* backendEnv = getenv("COMPOSITOR_BACKEND");
    Backend backend = UNKOWN;

    if (backendEnv == nullptr) {
        if (deviceinfo->driverType() == DeviceInfo::DriverType::Halium) {
            printf("Is android device, defaults to mir\n");
            backend = MIR;
        } else {
            printf("Is not android device, defaults to wayland\n");
            backend = WAYLAND;
        }
    } else {
        printf("Using COMPOSITOR_BACKEND varable to set backend\n");
        if (strcmp(backendEnv, "wayland") == 0) {
            backend = WAYLAND;
        }
        if (strcmp(backendEnv, "mir") == 0) {
            backend = MIR;
        }
    }

    switch(backend) {
        case MIR:
            printf("Using mir backend\n");
            surfaces = mir_eglapp_init(argc, argv);
            break;
        case WAYLAND:
            printf("Using wayland backend\n");
            surfaces = wayland_eglapp_init(argc, argv);
            break;
        default:
            printf("Uknown backend %s\n", backendEnv);
            return EXIT_FAILURE;
    }

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
    prog[ROBOT] = createShaderProgram(vShaderSrcPlain, fShaderSrcPlain);
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

    vpos[ROBOT]         = glGetAttribLocation(prog[ROBOT],  "aPosition");
    aTexCoords[ROBOT]   = glGetAttribLocation(prog[ROBOT],  "aTexCoords");
    sampler[ROBOT]      = glGetUniformLocation(prog[ROBOT], "uSampler");
    offset[ROBOT]       = glGetUniformLocation(prog[ROBOT], "uOffset");
    projMat[ROBOT]      = glGetUniformLocation(prog[ROBOT], "uProjMat");

    vpos[WHITE_DOT]         = glGetAttribLocation(prog[WHITE_DOT],  "aPosition");
    aTexCoords[WHITE_DOT]   = glGetAttribLocation(prog[WHITE_DOT],  "aTexCoords");
    sampler[WHITE_DOT]      = glGetUniformLocation(prog[WHITE_DOT], "uSampler");
    offset[WHITE_DOT]       = glGetUniformLocation(prog[WHITE_DOT], "uOffset");
    projMat[WHITE_DOT]      = glGetUniformLocation(prog[WHITE_DOT], "uProjMat");

    // create and upload spinner-artwork
    // note that the embedded image data has pre-multiplied alpha
    glGenTextures(MAX_TEXTURES, texture);
    uploadTexture(texture[WALLPAPER], wallpaper);
    uploadTexture(texture[ROBOT], robot);
    uploadTexture(texture[WHITE_DOT], white_dot);
    uploadTexture(texture[ORANGE_DOT], orange_dot);

    // bunch of shader-attributes to enable
    glVertexAttribPointer(aTexCoords[WALLPAPER], 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(vpos[WALLPAPER]);
    glEnableVertexAttribArray(aTexCoords[WALLPAPER]);
    glVertexAttribPointer(aTexCoords[ROBOT], 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(vpos[ROBOT]);
    glEnableVertexAttribArray(aTexCoords[ROBOT]);
    glVertexAttribPointer(aTexCoords[WHITE_DOT], 2, GL_FLOAT, GL_FALSE, 0, texCoords);
    glEnableVertexAttribArray(vpos[WHITE_DOT]);
    glEnableVertexAttribArray(aTexCoords[WHITE_DOT]);
    glActiveTexture(GL_TEXTURE0);

    AnimationValues anim = {0.0, 0.0, 0};
    GTimer* timer = g_timer_new();

    auto const pixels_per_gu = deviceinfo->gridUnit();
    auto const gu2px =
        [pixels_per_gu] (float gu)
        {
            return pixels_per_gu * gu;
        };
    auto const native_orientation = deviceinfo->primaryOrientation();

    std::cout << "Spinner using pixels per grid unit: " <<  pixels_per_gu << std::endl;
    std::cout << "Spinner using native orientation: '" << native_orientation << "'" << std::endl;

    while (mir_eglapp_running())
    {
        for (auto const& surface : surfaces)
            surface->paint([&](unsigned int width, unsigned int height)
            {
                bool const needs_rotation =
                    (width < height && native_orientation == "landscape") ||
                    (width > height && native_orientation == "portrait");

                GLfloat robotWidth = gu2px (14.5f);
                GLfloat robotHeight = gu2px (5.0f);
                GLfloat robotXOffset = gu2px (0.5f);
                GLfloat dotSize = gu2px (0.4f);
                GLfloat dotXGap = gu2px (2.5f);
                GLfloat dotYGap = gu2px (2.5f);

                auto render_width = width;
                auto render_height = height;
                if (needs_rotation)
                    std::swap(render_width, render_height);

                const GLfloat fullscreen[] = {
                    (GLfloat) render_width, 0.0f,
                    (GLfloat) render_width, (GLfloat) render_height,
                    0.0f,            0.0f,
                    0.0f,            (GLfloat) render_height
                };

                const GLfloat robot[] = {
                    robotWidth, 0.0f,
                    robotWidth, robotHeight,
                    0.0f,      0.0f,
                    0.0f,      robotHeight
                };

                const GLfloat dot[] = {
                    dotSize, 0.0f,
                    dotSize, dotSize,
                    0.0f,    0.0f,
                    0.0f,    dotSize
                };

                auto mvpMatrix = glm::mat4(1.0f);
                if (needs_rotation)
                    mvpMatrix = glm::rotate(mvpMatrix, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                mvpMatrix = glm::translate(mvpMatrix, glm::vec3(-1.0, -1.0, 0.0f));
                mvpMatrix = glm::scale(mvpMatrix,
                                       glm::vec3(2.0f / render_width, 2.0f / render_height, 1.0f));

                auto widthRatio = (1.0f * wallpaper.height * render_width) / (wallpaper.width * render_height);
                auto heightRatio = 1.0f;
                if (widthRatio > 1.0f) {
                    heightRatio = 1.0f / widthRatio;
                    widthRatio = 1.0f;
                }
                auto const wallpaperProjMatrix = glm::ortho(-widthRatio, widthRatio, heightRatio, -heightRatio, -1.0f, 1.0f);
                auto wallpaperMatrix = wallpaperProjMatrix * mvpMatrix;

                auto const projMatrix = glm::ortho(-1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f);
                mvpMatrix = projMatrix * mvpMatrix;

                glViewport(0, 0, width, height);

                glClearColor(BLACK, anim.fadeBackground);
                glClear(GL_COLOR_BUFFER_BIT);

                // draw wallpaper backdrop
                glVertexAttribPointer(vpos[WALLPAPER], 2, GL_FLOAT, GL_FALSE, 0, fullscreen);
                glUseProgram(prog[WALLPAPER]);
                glBindTexture(GL_TEXTURE_2D, texture[WALLPAPER]);
                glUniform1i(sampler[WALLPAPER], 0);
                glUniform2f(offset[WALLPAPER], 0.0f, 0.0f);
                glUniformMatrix4fv(projMat[WALLPAPER], 1, GL_FALSE, glm::value_ptr(wallpaperMatrix));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw robot
                glVertexAttribPointer(vpos[ROBOT], 2, GL_FLOAT, GL_FALSE, 0, robot);
                glUseProgram(prog[ROBOT]);
                glBindTexture(GL_TEXTURE_2D, texture[ROBOT]);
                glUniform1i(sampler[ROBOT], 0);
                glUniform2f(offset[ROBOT], render_width/2.0f - robotWidth / 2.0f + robotXOffset, render_height / 2.0f - robotHeight * 0.75f);
                glUniformMatrix4fv(projMat[ROBOT], 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw white/orange dots
                glVertexAttribPointer(vpos[WHITE_DOT], 2, GL_FLOAT, GL_FALSE, 0, dot);
                glUseProgram(prog[WHITE_DOT]);
                glUniform1i(sampler[WHITE_DOT], 0);
                glUniformMatrix4fv(projMat[WHITE_DOT], 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                for (int i = -2; i < 3; i++) {
                    glBindTexture(GL_TEXTURE_2D, texture[anim.dot_mask >> (i + 2) ? ORANGE_DOT : WHITE_DOT]);
                    glUniform2f(offset[WHITE_DOT], render_width/2.0f + i * dotXGap, render_height / 2.0f + robotHeight / 2.0f + dotYGap - robotHeight * 0.25f);
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
