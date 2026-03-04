/* ui.h - Immediate-mode UI controls */
#ifndef UI_H
#define UI_H

#include <glad/glad.h>
#include <stdbool.h>

/* UI interaction result */
typedef enum {
    UI_NONE = 0,
    UI_HOVERED,
    UI_PRESSED,
    UI_RELEASED,
    UI_DRAGGING,
} UIResult;

/* UI state context */
typedef struct {
    /* Input state */
    int mouse_x, mouse_y;
    bool mouse_down;
    bool mouse_clicked;   /* just pressed this frame */
    bool mouse_released;  /* just released this frame */

    /* Hot/active tracking */
    int hot_id;
    int active_id;

    /* Screen size */
    int screen_w, screen_h;

    /* Control bar state */
    float bar_alpha;       /* 0..1 current opacity */
    float bar_target;      /* 0 or 1 */
    float idle_timer;      /* seconds since last mouse move */
    bool  bar_hovered;     /* mouse is in bar region */

    /* Rendering resources */
    GLuint program;        /* UI shader */
    GLuint quad_vao, quad_vbo;
} UIContext;

/* Initialize UI system */
void ui_init(UIContext *ui);
void ui_destroy(UIContext *ui);

/* Call at start of frame with current input */
void ui_begin(UIContext *ui, int mx, int my, bool mouse_down,
              int screen_w, int screen_h, float dt);

/* Draw a filled rounded rect */
void ui_draw_rect(UIContext *ui, float x, float y, float w, float h,
                  float radius, float r, float g, float b, float a);

/* Button: returns true if clicked */
bool ui_button(UIContext *ui, int id, float x, float y, float w, float h,
               const char *label, bool active_state);

/* Slider: returns new value (0..1) */
float ui_slider(UIContext *ui, int id, float x, float y, float w, float h,
                float value);

/* Progress bar: returns new seek position if clicked, otherwise -1 */
float ui_progress_bar(UIContext *ui, int id, float x, float y, float w, float h,
                      float progress, float buffered);

/* Toggle switch: returns new state */
bool ui_toggle(UIContext *ui, int id, float x, float y, bool state);

/* Check if point is inside rect */
bool ui_point_in_rect(float px, float py, float x, float y, float w, float h);

/* Update control bar visibility (auto-hide after idle) */
void ui_update_bar(UIContext *ui, float dt, bool mouse_moved);

/* Call at end of frame */
void ui_end(UIContext *ui);

#endif /* UI_H */
