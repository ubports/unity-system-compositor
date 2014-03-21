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
#include <assert.h>
#include <cairo.h>
#include <stdio.h>
#include <GLES2/gl2.h>
#include <math.h>

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
#define MID_AUBERGINE   0.368627451f, 0.152941176f, 0.31372549f
#define ORANGE          0.866666667f, 0.282352941f, 0.141414141f
#define WARM_GREY       0.682352941f, 0.654901961f, 0.623529412f
#define COOL_GREY       0.2f,         0.2f,         0.2f
#define LIGHT_AUBERGINE 0.466666667f, 0.297297297f, 0.435294118f
#define DARK_AUBERGINE  0.17254902f,  0.0f,         0.117647059f
#define BLACK           0.0f,         0.0f,         0.0f
#define WHITE           1.0f,         1.0f,         1.0f
#define PI              3.141592654f

void identity (float* out)
{
    out[0]  = 1.0f;
    out[1]  = 0.0f;
    out[2]  = 0.0f;
    out[3]  = 0.0f;
    out[4]  = 0.0f;
    out[5]  = 1.0f;
    out[6]  = 0.0f;
    out[7]  = 0.0f;
    out[8]  = 0.0f;
    out[9]  = 0.0f;
    out[10] = 1.0f;
    out[11] = 0.0f;
    out[12] = 0.0f;
    out[13] = 0.0f;
    out[14] = 0.0f;
    out[15] = 1.0f;
}

void frustum (float a,
              float b,
              float c,
              float d,
              float e,
              float g,
              float* out)
{
    assert(out);

    float h = b - a;
    float i = d - c;
    float j = g - e;

    out[0]  = e * 2.0f / h;
    out[1]  = 0.0f;
    out[2]  = 0.0f;
    out[3]  = 0.0f;
    out[4]  = 0.0f;
    out[5]  = e * 2.0f / i;
    out[6]  = 0.0f;
    out[7]  = 0.0f;
    out[8]  = (b + a) / h;
    out[9]  = (d + c) / i;
    out[10] = -(g + e) / j;
    out[11] = -1.0f;
    out[12] = 0.0f;
    out[13] = 0.0f;
    out[14] = -(g * e * 2.0f) / j;
    out[15] = 0.0f;
}

void perspective (float a,
                  float b,
                  float c,
                  float d,
                  float* out)
{
    assert(out);

    a = c * tanf(a * PI / 360.0f);
    b = a * b;
    frustum (-b, b, -a, a, c, d, out);
}

cairo_surface_t* pngToSurface (const char* filename)
{
    // sanity check
    if (!filename)
        return NULL;

    // create surface from PNG
    cairo_surface_t* surface = NULL;
    surface = cairo_image_surface_create_from_png (filename);
    if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
        return NULL;

    return surface;
}

void uploadTexture(GLuint id, cairo_surface_t* surface)
{
    if (!id || !surface)
        return;

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

int main(int argc, char *argv[])
{
    const char vShaderSrcSpinner[] =
        "attribute vec4 vPosition;                       \n"
        "attribute vec2 aTexCoords;                      \n"
        "uniform float theta;                            \n"
        "uniform mat4 uPersp;                            \n"
        "varying vec2 vTexCoords;                        \n"
        "void main()                                     \n"
        "{                                               \n"
        "    float c = cos(theta);                       \n"
        "    float s = sin(theta);                       \n"
        "    mat2 m;                                     \n"
        "    m[0] = vec2(c, s);                          \n"
        "    m[1] = vec2(-s, c);                         \n"
        "    vec2 p = m * vec2(vPosition.x, vPosition.y);\n"
        "    gl_Position = uPersp * vec4(p, -1.0, 1.0);  \n"
        "    vTexCoords = aTexCoords;                    \n"
        "}                                               \n";

    const char fShaderSrc[] =
        "precision mediump float;                                    \n"
        "varying vec2 vTexCoords;                                    \n"
        "uniform sampler2D uSampler;                                 \n"
        "void main()                                                 \n"
        "{                                                           \n"
        "    // swizzle because texture was created with cairo       \n"
        "    vec4 col = texture2D(uSampler, vTexCoords).bgra;        \n"
        "    float a = col.a;                                        \n"
        "    // revert cairo's premultiplied alpha                   \n"
        "    gl_FragColor = vec4(col.r / a, col.g / a, col.b / a, a);\n"
        "}                                                           \n";

    const GLfloat vertices[] =
    {
        0.05f, 0.05f,
        0.05f, -0.05f,
        -0.05f, 0.05f,
        -0.05f, -0.05f,
    };

    const GLfloat texCoordsSpinner[] =
    {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
    };

    GLuint prog[1];
    GLuint texture[1];
    GLint vpos[1];
    GLint theta;
    GLint aTexCoords[1];
    GLint sampler[1];
    GLint uPersp[1];
    unsigned int width = 0, height = 0;
    GLfloat angle = 0.0f;

    if (!mir_eglapp_init(argc, argv, &width, &height))
        return 1;

    prog[0] = createShaderProgram (vShaderSrcSpinner, fShaderSrc);

    // setup viewport and projection
    glClearColor(BLACK, mir_eglapp_background_opacity);
    glViewport(0, 0, width, height);
    float persp[16];
    perspective (45.0f,
                 (float) width / (float) height,
                 0.1f,
                 100.0f,
                 persp);

    // setup proper GL-blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // get locations of shader-attributes/uniforms
    vpos[0] = glGetAttribLocation(prog[0], "vPosition");
    aTexCoords[0] = glGetAttribLocation(prog[0], "aTexCoords");
    theta = glGetUniformLocation(prog[0], "theta");
    sampler[0] = glGetUniformLocation(prog[0], "uSampler");
    uPersp[0] = glGetUniformLocation(prog[0], "uPersp");

    // create and upload spinner-artwork
    glGenTextures(1, texture);
    cairo_surface_t* spinner = pngToSurface (PKGDATADIR "/spinner.png");
    uploadTexture(texture[0], spinner);

    // bunch of shader-attributes to enable
    glVertexAttribPointer(vpos[0], 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(aTexCoords[0], 2, GL_FLOAT, GL_FALSE, 0, texCoordsSpinner);
    glEnableVertexAttribArray(vpos[0]);
    glEnableVertexAttribArray(aTexCoords[0]);
    glActiveTexture(GL_TEXTURE0);

    while (mir_eglapp_running())
    {
        glClear(GL_COLOR_BUFFER_BIT);

        // draw spinner
        glUseProgram(prog[0]);
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        glUniform1i(sampler[0], 0);
        glUniform1f(theta, angle);
        glUniformMatrix4fv(uPersp[0], 1, GL_FALSE, persp);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // update animation variable
        angle -= 0.04f;

        mir_eglapp_swap_buffers();
    }

    mir_eglapp_shutdown();

    glDeleteTextures(1, texture);

    return 0;
}
