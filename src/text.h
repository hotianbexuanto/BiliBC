/* text.h - FreeType glyph atlas manager */
#ifndef TEXT_H
#define TEXT_H

#include <glad/glad.h>
#include <stdbool.h>
#include <stdint.h>

/* Glyph info cached in atlas */
typedef struct {
    uint32_t codepoint;
    int      width, height;    /* Bitmap size */
    int      bearing_x, bearing_y;
    int      advance;          /* Horizontal advance in pixels */
    float    u0, v0, u1, v1;  /* UV coords in atlas */
    bool     valid;
} GlyphInfo;

/* Font context */
typedef struct FontContext FontContext;

/* Create a font context. font_path can be NULL to use system default. */
FontContext *font_ctx_create(const char *font_path, int base_size);
void font_ctx_destroy(FontContext *ctx);

/* Get glyph info, rasterizing on demand if needed */
const GlyphInfo *font_get_glyph(FontContext *ctx, uint32_t codepoint);

/* Get the atlas texture ID */
GLuint font_get_atlas_texture(const FontContext *ctx);

/* Get atlas dimensions */
void font_get_atlas_size(const FontContext *ctx, int *w, int *h);

/* Measure a UTF-8 string width in pixels */
float font_measure_text(FontContext *ctx, const char *text, float scale);

/* Get line height */
int font_get_line_height(const FontContext *ctx);

#endif /* TEXT_H */
