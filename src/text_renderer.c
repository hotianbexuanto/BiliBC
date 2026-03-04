/* text_renderer.c - Batch text renderer for danmaku */
#include "text_renderer.h"
#include "utf8.h"
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Vertex: pos(2) + texcoord(2) + color(4) = 8 floats */
#define FLOATS_PER_VERT 8
#define VERTS_PER_QUAD  6
#define MAX_QUADS       8192
#define VBO_SIZE        (MAX_QUADS * VERTS_PER_QUAD * FLOATS_PER_VERT * sizeof(float))

struct TextRenderer {
    FontContext *font;
    GLuint program;
    GLuint vao, vbo;

    float *vertices;      /* CPU vertex buffer */
    int quad_count;

    /* Uniforms */
    GLint u_projection;
    GLint u_atlas;

    int screen_w, screen_h;
};

static void make_ortho(float *m, float l, float r, float b, float t) {
    memset(m, 0, 16 * sizeof(float));
    m[0]  = 2.0f / (r - l);
    m[5]  = 2.0f / (t - b);
    m[10] = -1.0f;
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = 0.0f;
    m[15] = 1.0f;
}

TextRenderer *text_renderer_create(FontContext *font) {
    TextRenderer *tr = (TextRenderer *)calloc(1, sizeof(TextRenderer));
    tr->font = font;

    /* Load shader */
    tr->program = shader_load("shaders/danmaku.vert", "shaders/danmaku.frag");
    if (!tr->program) {
        fprintf(stderr, "[TextRenderer] Failed to load shaders\n");
        free(tr);
        return NULL;
    }

    tr->u_projection = glGetUniformLocation(tr->program, "uProjection");
    tr->u_atlas = glGetUniformLocation(tr->program, "uAtlas");

    /* Create VAO/VBO */
    glGenVertexArrays(1, &tr->vao);
    glGenBuffers(1, &tr->vbo);
    glBindVertexArray(tr->vao);
    glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);
    glBufferData(GL_ARRAY_BUFFER, VBO_SIZE, NULL, GL_DYNAMIC_DRAW);

    /* Position */
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERT * sizeof(float), (void *)0);
    /* TexCoord */
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, FLOATS_PER_VERT * sizeof(float),
                          (void *)(2 * sizeof(float)));
    /* Color */
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, FLOATS_PER_VERT * sizeof(float),
                          (void *)(4 * sizeof(float)));

    glBindVertexArray(0);

    tr->vertices = (float *)malloc(VBO_SIZE);

    return tr;
}

void text_renderer_destroy(TextRenderer *tr) {
    if (!tr) return;
    if (tr->program) glDeleteProgram(tr->program);
    if (tr->vao) glDeleteVertexArrays(1, &tr->vao);
    if (tr->vbo) glDeleteBuffers(1, &tr->vbo);
    free(tr->vertices);
    free(tr);
}

void text_renderer_begin(TextRenderer *tr, int screen_w, int screen_h) {
    tr->quad_count = 0;
    tr->screen_w = screen_w;
    tr->screen_h = screen_h;
}

static void push_quad(TextRenderer *tr,
                      float x0, float y0, float x1, float y1,
                      float u0, float v0, float u1, float v1,
                      float r, float g, float b, float a)
{
    if (tr->quad_count >= MAX_QUADS) return;
    float *v = tr->vertices + tr->quad_count * VERTS_PER_QUAD * FLOATS_PER_VERT;

    /* Triangle 1 */
    /* Top-left */
    v[0] = x0; v[1] = y0; v[2] = u0; v[3] = v0; v[4] = r; v[5] = g; v[6] = b; v[7] = a;
    v += FLOATS_PER_VERT;
    /* Bottom-left */
    v[0] = x0; v[1] = y1; v[2] = u0; v[3] = v1; v[4] = r; v[5] = g; v[6] = b; v[7] = a;
    v += FLOATS_PER_VERT;
    /* Bottom-right */
    v[0] = x1; v[1] = y1; v[2] = u1; v[3] = v1; v[4] = r; v[5] = g; v[6] = b; v[7] = a;
    v += FLOATS_PER_VERT;

    /* Triangle 2 */
    /* Top-left */
    v[0] = x0; v[1] = y0; v[2] = u0; v[3] = v0; v[4] = r; v[5] = g; v[6] = b; v[7] = a;
    v += FLOATS_PER_VERT;
    /* Bottom-right */
    v[0] = x1; v[1] = y1; v[2] = u1; v[3] = v1; v[4] = r; v[5] = g; v[6] = b; v[7] = a;
    v += FLOATS_PER_VERT;
    /* Top-right */
    v[0] = x1; v[1] = y0; v[2] = u1; v[3] = v0; v[4] = r; v[5] = g; v[6] = b; v[7] = a;

    tr->quad_count++;
}

void text_renderer_draw(TextRenderer *tr, const char *text,
                        float x, float y, float scale,
                        uint32_t color, float alpha)
{
    float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >>  8) & 0xFF) / 255.0f;
    float b = ((color      ) & 0xFF) / 255.0f;

    float cursor_x = x;
    const char *p = text;
    uint32_t cp;

    while ((cp = utf8_decode(&p)) != 0) {
        const GlyphInfo *gi = font_get_glyph(tr->font, cp);
        if (!gi || !gi->valid || gi->width == 0) {
            if (gi) cursor_x += gi->advance * scale;
            continue;
        }

        float xpos = cursor_x + gi->bearing_x * scale;
        float ypos = y - gi->bearing_y * scale;
        float w = gi->width * scale;
        float h = gi->height * scale;

        push_quad(tr,
                  xpos, ypos, xpos + w, ypos + h,
                  gi->u0, gi->v0, gi->u1, gi->v1,
                  r, g, b, alpha);

        cursor_x += gi->advance * scale;
    }
}

void text_renderer_end(TextRenderer *tr) {
    if (tr->quad_count == 0) return;

    /* Upload vertices */
    int vert_count = tr->quad_count * VERTS_PER_QUAD;
    int byte_count = vert_count * FLOATS_PER_VERT * sizeof(float);

    glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, byte_count, tr->vertices);

    /* Setup state */
    glUseProgram(tr->program);

    float proj[16];
    make_ortho(proj, 0, (float)tr->screen_w, (float)tr->screen_h, 0);
    glUniformMatrix4fv(tr->u_projection, 1, GL_FALSE, proj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font_get_atlas_texture(tr->font));
    glUniform1i(tr->u_atlas, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(tr->vao);
    glDrawArrays(GL_TRIANGLES, 0, vert_count);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}
