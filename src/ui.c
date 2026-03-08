/* ui.c - Immediate-mode UI controls */
#include "ui.h"
#include "shader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BAR_HEIGHT 80.0f
#define BAR_FADE_SPEED 5.0f
#define IDLE_TIMEOUT 3.0f

static void create_quad(UIContext *ui) {
    float verts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    glGenVertexArrays(1, &ui->quad_vao);
    glGenBuffers(1, &ui->quad_vbo);
    glBindVertexArray(ui->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, ui->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void ui_init(UIContext *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->bar_alpha = 1.0f;
    ui->bar_target = 1.0f;
    ui->hot_id = -1;
    ui->active_id = -1;

    ui->program = shader_load("shaders/blur_common.vert", "shaders/ui.frag");
    create_quad(ui);
}

void ui_destroy(UIContext *ui) {
    if (ui->program) glDeleteProgram(ui->program);
    if (ui->quad_vao) glDeleteVertexArrays(1, &ui->quad_vao);
    if (ui->quad_vbo) glDeleteBuffers(1, &ui->quad_vbo);
}

void ui_begin(UIContext *ui, int mx, int my, bool mouse_down,
              int screen_w, int screen_h, float dt)
{
    ui->mouse_clicked = mouse_down && !ui->mouse_down;
    ui->mouse_released = !mouse_down && ui->mouse_down;
    ui->mouse_x = mx;
    ui->mouse_y = my;
    ui->mouse_down = mouse_down;
    ui->screen_w = screen_w;
    ui->screen_h = screen_h;
    ui->hot_id = -1;

    /* Check if mouse is in control bar region */
    ui->bar_hovered = (my >= screen_h - (int)BAR_HEIGHT);

    (void)dt;
}

bool ui_point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

void ui_draw_rect(UIContext *ui, float x, float y, float w, float h,
                  float radius, float r, float g, float b, float a)
{
    if (a <= 0.001f) return;

    glUseProgram(ui->program);

    GLint u_rect = glGetUniformLocation(ui->program, "uRect");
    GLint u_radius = glGetUniformLocation(ui->program, "uRadius");
    GLint u_color = glGetUniformLocation(ui->program, "uColor");
    GLint u_border = glGetUniformLocation(ui->program, "uBorderColor");
    GLint u_bw = glGetUniformLocation(ui->program, "uBorderWidth");
    GLint u_res = glGetUniformLocation(ui->program, "uResolution");

    glUniform4f(u_rect, x, y, w, h);
    glUniform1f(u_radius, radius);
    glUniform4f(u_color, r, g, b, a);
    glUniform4f(u_border, 0, 0, 0, 0);
    glUniform1f(u_bw, 0);
    glUniform2f(u_res, (float)ui->screen_w, (float)ui->screen_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(ui->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glUseProgram(0);
}

bool ui_button(UIContext *ui, int id, float x, float y, float w, float h,
               const char *label, bool active_state)
{
    (void)label;
    bool hovered = ui_point_in_rect((float)ui->mouse_x, (float)ui->mouse_y, x, y, w, h);
    bool clicked = false;

    if (hovered) {
        ui->hot_id = id;
        if (ui->mouse_clicked) {
            ui->active_id = id;
        }
    }

    if (ui->active_id == id && ui->mouse_released) {
        if (hovered) clicked = true;
        ui->active_id = -1;
    }

    /* Draw button */
    float r = 0.2f, g = 0.2f, b = 0.25f;  /* Darker base color */
    float alpha = 0.6f * ui->bar_alpha;

    if (active_state) {
        r = 0.2f; g = 0.6f; b = 1.0f;  /* Brighter blue for active */
        alpha = 0.9f * ui->bar_alpha;
    }
    if (hovered) {
        alpha += 0.15f;
        if (!active_state) {
            r += 0.1f; g += 0.1f; b += 0.1f;  /* Lighten on hover */
        }
    }
    if (ui->active_id == id) alpha += 0.1f;

    ui_draw_rect(ui, x, y, w, h, 8.0f, r, g, b, alpha);  /* Larger radius: 8px */

    return clicked;
}

float ui_slider(UIContext *ui, int id, float x, float y, float w, float h,
                float value)
{
    bool hovered = ui_point_in_rect((float)ui->mouse_x, (float)ui->mouse_y,
                                     x - 5, y - 5, w + 10, h + 10);

    if (hovered && ui->mouse_clicked) {
        ui->active_id = id;
    }

    if (ui->active_id == id) {
        if (ui->mouse_down) {
            value = ((float)ui->mouse_x - x) / w;
            if (value < 0) value = 0;
            if (value > 1) value = 1;
        } else {
            ui->active_id = -1;
        }
    }

    /* Draw track */
    float alpha = ui->bar_alpha;
    ui_draw_rect(ui, x, y + h * 0.35f, w, h * 0.3f, 4.0f,  /* 4px radius */
                 0.3f, 0.3f, 0.35f, 0.5f * alpha);

    /* Draw filled portion */
    ui_draw_rect(ui, x, y + h * 0.35f, w * value, h * 0.3f, 4.0f,
                 0.2f, 0.6f, 1.0f, 0.85f * alpha);  /* Brighter blue */

    /* Draw handle */
    float handle_x = x + w * value - h * 0.3f;
    float handle_size = h * 0.6f;
    if (hovered || ui->active_id == id) {
        ui_draw_rect(ui, handle_x, y + h * 0.2f, handle_size, handle_size,
                     handle_size * 0.5f, 1.0f, 1.0f, 1.0f, 0.95f * alpha);
    }

    return value;
}

float ui_progress_bar(UIContext *ui, int id, float x, float y, float w, float h,
                      float progress, float buffered)
{
    bool hovered = ui_point_in_rect((float)ui->mouse_x, (float)ui->mouse_y,
                                     x, y - 5, w, h + 10);
    float seek_pos = -1.0f;

    if (hovered && ui->mouse_clicked) {
        ui->active_id = id;
    }

    if (ui->active_id == id) {
        float pos = ((float)ui->mouse_x - x) / w;
        if (pos < 0) pos = 0;
        if (pos > 1) pos = 1;

        if (ui->mouse_released) {
            seek_pos = pos;
            ui->active_id = -1;
        }
    }

    float alpha = ui->bar_alpha;
    float bar_h = hovered ? h * 1.5f : h;
    float bar_y = y + (h - bar_h) * 0.5f;

    /* Background track */
    ui_draw_rect(ui, x, bar_y, w, bar_h, 4.0f,  /* 4px radius */
                 0.25f, 0.25f, 0.3f, 0.5f * alpha);

    /* Buffered */
    if (buffered > 0) {
        ui_draw_rect(ui, x, bar_y, w * buffered, bar_h, 4.0f,
                     0.4f, 0.4f, 0.45f, 0.4f * alpha);
    }

    /* Progress */
    ui_draw_rect(ui, x, bar_y, w * progress, bar_h, 4.0f,
                 0.2f, 0.6f, 1.0f, 0.95f * alpha);  /* Brighter blue */

    /* Handle dot */
    if (hovered || ui->active_id == id) {
        float cx = x + w * progress;
        float dot = bar_h * 2.0f;
        ui_draw_rect(ui, cx - dot * 0.5f, bar_y - dot * 0.25f, dot, dot,
                     dot * 0.5f, 0.3f, 0.7f, 1.0f, alpha);  /* Blue handle */
    }

    return seek_pos;
}

bool ui_toggle(UIContext *ui, int id, float x, float y, bool state) {
    float w = 40.0f, h = 22.0f;
    bool hovered = ui_point_in_rect((float)ui->mouse_x, (float)ui->mouse_y, x, y, w, h);

    if (hovered && ui->mouse_clicked) {
        ui->active_id = id;
    }
    if (ui->active_id == id && ui->mouse_released) {
        if (hovered) state = !state;
        ui->active_id = -1;
    }

    float alpha = ui->bar_alpha;

    /* Track */
    if (state) {
        ui_draw_rect(ui, x, y, w, h, h * 0.5f, 0.2f, 0.6f, 1.0f, 0.85f * alpha);  /* Brighter blue */
    } else {
        ui_draw_rect(ui, x, y, w, h, h * 0.5f, 0.3f, 0.3f, 0.35f, 0.5f * alpha);  /* Darker gray */
    }

    /* Thumb */
    float thumb_x = state ? x + w - h : x;
    ui_draw_rect(ui, thumb_x + 2, y + 2, h - 4, h - 4, (h - 4) * 0.5f,
                 1.0f, 1.0f, 1.0f, 0.95f * alpha);

    return state;
}

void ui_update_bar(UIContext *ui, float dt, bool mouse_moved) {
    if (mouse_moved) {
        ui->idle_timer = 0;
        ui->bar_target = 1.0f;
    } else {
        ui->idle_timer += dt;
    }

    /* Auto-hide after idle timeout, unless mouse is in bar */
    if (ui->idle_timer > IDLE_TIMEOUT && !ui->bar_hovered && ui->active_id == -1) {
        ui->bar_target = 0.0f;
    }

    /* Smooth fade */
    float diff = ui->bar_target - ui->bar_alpha;
    ui->bar_alpha += diff * BAR_FADE_SPEED * dt;
    if (ui->bar_alpha < 0.001f) ui->bar_alpha = 0.0f;
    if (ui->bar_alpha > 0.999f) ui->bar_alpha = 1.0f;
}

void ui_end(UIContext *ui) {
    /* Reset click state */
    ui->mouse_clicked = false;
    ui->mouse_released = false;
}
