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
#if HAVE_PROPS
#include <hybris/properties/properties.h>
#endif
#include <signal.h>

#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>
#include <iostream>

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "wallpaper.h"
#include "logo.h"
#include "white_dot.h"
#include "orange_dot.h"

enum TextureIds {
    WALLPAPER = 0,
    LOGO,
    WHITE_DOT,
    ORANGE_DOT,
    MAX_TEXTURES
};

class SessionConfig
{
public:
    SessionConfig()
    {
        parse_session_conf_file();
    }

    int get_int(std::string const& key, int default_value)
    {
        try
        {
            if (conf_map.find(key) != conf_map.end())
                return std::stoi(conf_map[key]);
        }
        catch (...)
        {
        }
        
        return default_value;
    }

    std::string get_string(std::string const& key, std::string const& default_value)
    {
        return conf_map.find(key) != conf_map.end() ? conf_map[key] : default_value;
    }

private:
    void parse_session_conf_file()
    {
        std::ifstream fs{default_file};
        if (!fs.is_open())
        {
            fs.clear();
    #ifdef HAVE_PROPS
            char const* default_value = "";
            char value[PROP_VALUE_MAX];
            property_get(device_property_key, value, default_value);
    #endif
            fs.open(file_base + value + file_extension);
        }

        std::string line;
        while (std::getline(fs, line))
            conf_map.insert(parse_key_value_pair(line));
    }

    std::string trim(std::string const& s)
    {
       auto const wsfront = std::find_if_not(s.begin(), s.end(), [](int c) { return std::isspace(c); });
       auto const wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c) { return std::isspace(c); }).base();
       return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
    }

    std::pair<std::string,std::string> parse_key_value_pair(std::string kv)
    {
        auto const separator = kv.find("=");
        auto const key = kv.substr(0, separator);
        auto const value = separator != std::string::npos ? 
                           kv.substr(separator + 1, std::string::npos) :
                           std::string{};

        return {trim(key), trim(value)};
    }

    std::string const default_file{"/etc/ubuntu-touch-session.d/android.conf"};
    std::string const file_base{"/etc/ubuntu-touch-session.d/"};
    std::string const file_extension{".conf"};
    char const* device_property_key = "ro.product.device";
    std::map<std::string,std::string> conf_map;
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

    SessionConfig session_config;

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

    auto const pixels_per_gu = session_config.get_int("GRID_UNIT_PX", 13);
    auto const gu2px =
        [pixels_per_gu] (float gu)
        { 
            return pixels_per_gu * gu;
        };
    auto const native_orientation = session_config.get_string("NATIVE_ORIENTATION", "");

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

                GLfloat logoWidth = gu2px (14.5f);
                GLfloat logoHeight = gu2px (3.0f);
                GLfloat logoXOffset = gu2px (1.0f);
                GLfloat dotSize = gu2px (0.5f);
                GLfloat dotXGap = gu2px (2.5f);
                GLfloat dotYGap = gu2px (2.0f);
                
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

                auto mvpMatrix = glm::mat4(1.0f);
                if (needs_rotation)
                    mvpMatrix = glm::rotate(mvpMatrix, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                mvpMatrix = glm::translate(mvpMatrix, glm::vec3(-1.0, -1.0, 0.0f));
                mvpMatrix = glm::scale(mvpMatrix,
                                       glm::vec3(2.0f / render_width, 2.0f / render_height, 1.0f));
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
                glUniformMatrix4fv(projMat[WALLPAPER], 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw logo
                glVertexAttribPointer(vpos[LOGO], 2, GL_FLOAT, GL_FALSE, 0, logo);
                glUseProgram(prog[LOGO]);
                glBindTexture(GL_TEXTURE_2D, texture[LOGO]);
                glUniform1i(sampler[LOGO], 0);
                glUniform2f(offset[LOGO], render_width/2.0f - logoWidth / 2.0f + logoXOffset, render_height / 2.0f - logoHeight * 0.75f);
                glUniformMatrix4fv(projMat[LOGO], 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                // draw white/orange dots
                glVertexAttribPointer(vpos[WHITE_DOT], 2, GL_FLOAT, GL_FALSE, 0, dot);
                glUseProgram(prog[WHITE_DOT]);
                glUniform1i(sampler[WHITE_DOT], 0);
                glUniformMatrix4fv(projMat[WHITE_DOT], 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                for (int i = -2; i < 3; i++) {
                    glBindTexture(GL_TEXTURE_2D, texture[anim.dot_mask >> (i + 2) ? ORANGE_DOT : WHITE_DOT]);
                    glUniform2f(offset[WHITE_DOT], render_width/2.0f + i * dotXGap, render_height / 2.0f + logoHeight / 2.0f + dotYGap - logoHeight * 0.25f);
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
