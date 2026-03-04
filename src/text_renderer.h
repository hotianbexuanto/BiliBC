/* text_renderer.h - Batch text renderer for danmaku */
#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "text.h"
#include <glad/glad.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct TextRenderer TextRenderer;

/* Create a text renderer. Requires a font context. */
TextRenderer *text_renderer_create(FontContext *font);
void text_renderer_destroy(TextRenderer *tr);

/* Begin a new batch frame */
void text_renderer_begin(TextRenderer *tr, int screen_w, int screen_h);

/* Add a string to the batch */
void text_renderer_draw(TextRenderer *tr, const char *text,
                        float x, float y, float scale,
                        uint32_t color, float alpha);

/* Flush and render all queued text */
void text_renderer_end(TextRenderer *tr);

#endif /* TEXT_RENDERER_H */
