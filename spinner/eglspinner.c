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
#include <stdio.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <pango/pangocairo.h>

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

cairo_surface_t* textToSurface (const char* string,
                                const char* font,
                                double dpi,
                                int size,
                                float r,
                                float g,
                                float b,
                                float a,
                                float* aspect)
{
    // sanity check
    if (!string || !font || dpi < 1.0 || size < 1)
        return NULL;

    // create scratch surface
    cairo_surface_t* tmpSurf = NULL;
    tmpSurf = cairo_image_surface_create (CAIRO_FORMAT_A1, 1, 1);
    if (cairo_surface_status (tmpSurf) != CAIRO_STATUS_SUCCESS)
        return NULL;

    // create scratch context
    cairo_t* tmpCr = NULL;
    tmpCr = cairo_create (tmpSurf);
    cairo_surface_destroy (tmpSurf);
    if (cairo_status (tmpCr) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy (tmpSurf);
        return NULL;
    }

    // determine needed texture-size in pixels
    PangoLayout* layout = NULL;
    layout = pango_cairo_create_layout (tmpCr);
    PangoFontDescription* desc = NULL;
    desc = pango_font_description_from_string (font);
    pango_cairo_context_set_resolution (pango_layout_get_context (layout), dpi);
    pango_layout_context_changed (layout);
    pango_font_description_set_size (desc, size * PANGO_SCALE);
    pango_font_description_set_weight (desc, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_width (layout, -1);
    pango_layout_set_height (layout, -1);
    pango_layout_set_text (layout, string, -1);
    PangoRectangle logRect = {0, 0, 0, 0};
    pango_layout_get_extents (layout, NULL, &logRect);
    int width = PANGO_PIXELS (logRect.width);
    int height = PANGO_PIXELS (logRect.height);

    // clean up scratch resources
    g_object_unref (layout);
    cairo_destroy (tmpCr);

    // create surface for texture-creation
    cairo_surface_t* surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
        return NULL;

    // create scratch context
    cairo_t* cr = cairo_create (surface);
    cairo_surface_destroy (surface);
    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy (surface);
        return NULL;
    }

    // clear drawing-context
    cairo_scale (cr, 1.0f, 1.0f);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

    // set attributes for the font-layout
    layout = pango_cairo_create_layout (cr);
    desc = pango_font_description_from_string (font);
    pango_font_description_set_size (desc, size * PANGO_SCALE);
    pango_font_description_set_weight (desc, PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description (layout, desc);
    pango_font_description_free (desc);
    pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);
    pango_layout_set_width (layout, width * PANGO_SCALE);
    pango_layout_set_height (layout, height * PANGO_SCALE);
    pango_layout_set_text (layout, string, -1);
    pango_layout_get_extents (layout, NULL, &logRect);
    *aspect = (float) logRect.width  / (float) logRect.height;
    pango_cairo_context_set_resolution (pango_layout_get_context (layout), dpi);
    pango_layout_context_changed (layout);

    // do the rendering of the text itself
    cairo_move_to (cr, 0.0f, 0.0f);
    cairo_set_source_rgba (cr, r, g , b, a);
    pango_cairo_show_layout (cr, layout);

    // clean up scratch resources
    g_object_unref (layout);

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

    const char vShaderSrcText[] =
        "attribute vec4 vPosition;                                         \n"
        "attribute vec2 aTexCoords;                                        \n"
        "uniform float uAspect;                                            \n"
        "uniform mat4 uPersp;                                              \n"
        "varying vec2 vTexCoords;                                          \n"
        "void main()                                                       \n"
        "{                                                                 \n"
        "    vec2 p = 0.5 * vec2(vPosition.x * uAspect, vPosition.y - 0.2);\n"
        "    gl_Position = uPersp * vec4(p, -1.0, 1.0);                    \n"
        "    vTexCoords = aTexCoords;                                      \n"
        "}                                                                 \n";

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

    const GLfloat texCoordsText[] =
    {
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
    };

    GLuint prog[2];
    GLuint texture[2];
    GLint vpos[2];
    GLint theta;
    GLint aTexCoords[2];
    GLint sampler[2];
    GLint uPersp[2];
    GLint uAspect;
    unsigned int width = 0, height = 0;
    GLfloat angle = 0.0f;

    if (!mir_eglapp_init(argc, argv, &width, &height))
        return 1;

    prog[0] = createShaderProgram (vShaderSrcSpinner, fShaderSrc);
    prog[1] = createShaderProgram (vShaderSrcText, fShaderSrc);

    // setup viewport and projection
    glClearColor(MID_AUBERGINE, mir_eglapp_background_opacity);
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
    vpos[1] = glGetAttribLocation(prog[1], "vPosition");
    aTexCoords[1] = glGetAttribLocation(prog[1], "aTexCoords");
    sampler[1] = glGetUniformLocation(prog[1], "uSampler");
    uPersp[1] = glGetUniformLocation(prog[1], "uPersp");
    uAspect = glGetUniformLocation(prog[1], "uAspect");

    // create and upload spinner-artwork
    glGenTextures(2, texture);
    float aspect = 1.0f;
    cairo_surface_t* spinner = pngToSurface (PKGDATADIR "/spinner.png");
    uploadTexture(texture[0], spinner);

    // create and upload text-texture
    cairo_surface_t* text = textToSurface ("Loading...",
                                           "Ubuntu",
                                           200.0,
                                           32,
                                           WHITE,
                                           1.0f,
                                           &aspect);
    uploadTexture(texture[1], text);

    // bunch of shader-attributes to enable
    glVertexAttribPointer(vpos[0], 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(vpos[1], 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(aTexCoords[0], 2, GL_FLOAT, GL_FALSE, 0, texCoordsSpinner);
    glVertexAttribPointer(aTexCoords[1], 2, GL_FLOAT, GL_FALSE, 0, texCoordsText);
    glEnableVertexAttribArray(vpos[0]);
    glEnableVertexAttribArray(aTexCoords[0]);
    glEnableVertexAttribArray(vpos[1]);
    glEnableVertexAttribArray(aTexCoords[1]);
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

        // draw text
        glUseProgram(prog[1]);
        glBindTexture(GL_TEXTURE_2D, texture[1]);
        glUniform1i(sampler[1], 0);
        glUniform1f(uAspect, aspect);
        glUniformMatrix4fv(uPersp[1], 1, GL_FALSE, persp);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // update animation variable
        angle -= 0.04f;

        mir_eglapp_swap_buffers();
    }

    mir_eglapp_shutdown();

    glDeleteTextures(2, texture);

    return 0;
}
