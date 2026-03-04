/* blur.c - Kawase dual blur pipeline */
#include "blur.h"
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct BlurPipeline {
    /* FBO chain for downsample + upsample */
    GLuint fbo[BLUR_PASSES * 2];
    GLuint tex[BLUR_PASSES * 2];
    int    widths[BLUR_PASSES * 2];
    int    heights[BLUR_PASSES * 2];
    int    count;  /* number of FBOs */

    GLuint prog_down;  /* downsample shader */
    GLuint prog_up;    /* upsample shader */

    /* Fullscreen quad */
    GLuint quad_vao, quad_vbo;
};

static void create_quad(BlurPipeline *bp) {
    float verts[] = {
        /* pos        texcoord */
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    glGenVertexArrays(1, &bp->quad_vao);
    glGenBuffers(1, &bp->quad_vbo);
    glBindVertexArray(bp->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, bp->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
}

static void create_fbo(GLuint *fbo, GLuint *tex, int w, int h) {
    glGenFramebuffers(1, fbo);
    glGenTextures(1, tex);

    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

BlurPipeline *blur_create(int width, int height) {
    BlurPipeline *bp = (BlurPipeline *)calloc(1, sizeof(BlurPipeline));

    bp->prog_down = shader_load("shaders/blur_common.vert", "shaders/blur_down.frag");
    bp->prog_up   = shader_load("shaders/blur_common.vert", "shaders/blur_up.frag");

    if (!bp->prog_down || !bp->prog_up) {
        fprintf(stderr, "[Blur] Failed to load shaders\n");
        free(bp);
        return NULL;
    }

    create_quad(bp);
    blur_resize(bp, width, height);

    return bp;
}

void blur_destroy(BlurPipeline *bp) {
    if (!bp) return;
    for (int i = 0; i < bp->count; i++) {
        glDeleteFramebuffers(1, &bp->fbo[i]);
        glDeleteTextures(1, &bp->tex[i]);
    }
    if (bp->prog_down) glDeleteProgram(bp->prog_down);
    if (bp->prog_up) glDeleteProgram(bp->prog_up);
    if (bp->quad_vao) glDeleteVertexArrays(1, &bp->quad_vao);
    if (bp->quad_vbo) glDeleteBuffers(1, &bp->quad_vbo);
    free(bp);
}

void blur_resize(BlurPipeline *bp, int width, int height) {
    /* Destroy old FBOs */
    for (int i = 0; i < bp->count; i++) {
        glDeleteFramebuffers(1, &bp->fbo[i]);
        glDeleteTextures(1, &bp->tex[i]);
    }

    /* Create BLUR_PASSES downsample + BLUR_PASSES upsample FBOs */
    bp->count = BLUR_PASSES * 2;
    int w = width, h = height;

    /* Downsample chain */
    for (int i = 0; i < BLUR_PASSES; i++) {
        w = (w + 1) / 2;
        h = (h + 1) / 2;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        bp->widths[i] = w;
        bp->heights[i] = h;
        create_fbo(&bp->fbo[i], &bp->tex[i], w, h);
    }

    /* Upsample chain (reverse sizes) */
    for (int i = 0; i < BLUR_PASSES; i++) {
        int src = BLUR_PASSES - 2 - i;
        if (src < 0) {
            w = width;
            h = height;
        } else {
            w = bp->widths[src];
            h = bp->heights[src];
        }
        int idx = BLUR_PASSES + i;
        bp->widths[idx] = w;
        bp->heights[idx] = h;
        create_fbo(&bp->fbo[idx], &bp->tex[idx], w, h);
    }
}

static void draw_quad(BlurPipeline *bp) {
    glBindVertexArray(bp->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

GLuint blur_apply(BlurPipeline *bp, GLuint source_tex, int width, int height) {
    (void)width; (void)height;

    /* --- Downsample --- */
    glUseProgram(bp->prog_down);
    GLint u_tex = glGetUniformLocation(bp->prog_down, "uTexture");
    GLint u_hp = glGetUniformLocation(bp->prog_down, "uHalfPixel");

    GLuint input_tex = source_tex;
    for (int i = 0; i < BLUR_PASSES; i++) {
        int w = bp->widths[i];
        int h = bp->heights[i];

        glBindFramebuffer(GL_FRAMEBUFFER, bp->fbo[i]);
        glViewport(0, 0, w, h);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input_tex);
        glUniform1i(u_tex, 0);

        /* Half pixel of the INPUT texture size */
        int iw = (i == 0) ? width : bp->widths[i - 1];
        int ih = (i == 0) ? height : bp->heights[i - 1];
        glUniform2f(u_hp, 0.5f / (float)iw, 0.5f / (float)ih);

        draw_quad(bp);
        input_tex = bp->tex[i];
    }

    /* --- Upsample --- */
    glUseProgram(bp->prog_up);
    u_tex = glGetUniformLocation(bp->prog_up, "uTexture");
    u_hp = glGetUniformLocation(bp->prog_up, "uHalfPixel");

    input_tex = bp->tex[BLUR_PASSES - 1]; /* smallest */
    for (int i = 0; i < BLUR_PASSES; i++) {
        int idx = BLUR_PASSES + i;
        int w = bp->widths[idx];
        int h = bp->heights[idx];

        glBindFramebuffer(GL_FRAMEBUFFER, bp->fbo[idx]);
        glViewport(0, 0, w, h);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input_tex);
        glUniform1i(u_tex, 0);

        int src_idx = (i == 0) ? (BLUR_PASSES - 1) : (BLUR_PASSES + i - 1);
        glUniform2f(u_hp, 0.5f / (float)bp->widths[src_idx], 0.5f / (float)bp->heights[src_idx]);

        draw_quad(bp);
        input_tex = bp->tex[idx];
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return bp->tex[bp->count - 1];
}

GLuint blur_get_result(const BlurPipeline *bp) {
    return bp->tex[bp->count - 1];
}
