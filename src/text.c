/* text.c - FreeType glyph atlas with dual-channel stroke+fill */
#include "text.h"
#include "utf8.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ATLAS_W 2048
#define ATLAS_H 2048
#define GLYPH_PAD 2
#define STROKE_RADIUS 2  /* px */

/* Hash map for glyph lookup */
#define GLYPH_MAP_SIZE 4096

typedef struct GlyphNode {
    GlyphInfo info;
    struct GlyphNode *next;
} GlyphNode;

struct FontContext {
    FT_Library  ft;
    FT_Face     face;
    FT_Stroker  stroker;
    int         base_size;
    int         line_height;

    /* Atlas */
    GLuint  atlas_tex;
    int     cursor_x, cursor_y;  /* Current packing position */
    int     row_height;          /* Tallest glyph in current row */
    uint8_t *atlas_data;         /* CPU copy for upload (2 channels: RG) */

    /* Glyph cache */
    GlyphNode *map[GLYPH_MAP_SIZE];
};

static unsigned int hash_cp(uint32_t cp) {
    return cp % GLYPH_MAP_SIZE;
}

static GlyphInfo *find_glyph(FontContext *ctx, uint32_t cp) {
    unsigned int h = hash_cp(cp);
    for (GlyphNode *n = ctx->map[h]; n; n = n->next) {
        if (n->info.codepoint == cp) return &n->info;
    }
    return NULL;
}

static GlyphInfo *insert_glyph(FontContext *ctx, uint32_t cp) {
    unsigned int h = hash_cp(cp);
    GlyphNode *n = (GlyphNode *)calloc(1, sizeof(GlyphNode));
    n->info.codepoint = cp;
    n->next = ctx->map[h];
    ctx->map[h] = n;
    return &n->info;
}

/* Rasterize a glyph: stroke in R channel, fill in G channel */
static bool rasterize_glyph(FontContext *ctx, uint32_t cp, GlyphInfo *gi) {
    FT_UInt glyph_idx = FT_Get_Char_Index(ctx->face, cp);
    if (glyph_idx == 0 && cp != 0) {
        gi->valid = false;
        return false;
    }

    if (FT_Load_Glyph(ctx->face, glyph_idx, FT_LOAD_DEFAULT)) {
        gi->valid = false;
        return false;
    }

    /* Get fill bitmap */
    FT_Glyph glyph_fill;
    FT_Get_Glyph(ctx->face->glyph, &glyph_fill);
    FT_Glyph_To_Bitmap(&glyph_fill, FT_RENDER_MODE_NORMAL, NULL, 1);
    FT_BitmapGlyph bmp_fill = (FT_BitmapGlyph)glyph_fill;

    /* Get stroke bitmap */
    FT_Glyph glyph_stroke;
    FT_Get_Glyph(ctx->face->glyph, &glyph_stroke);
    FT_Glyph_StrokeBorder(&glyph_stroke, ctx->stroker, 0, 1);
    FT_Glyph_To_Bitmap(&glyph_stroke, FT_RENDER_MODE_NORMAL, NULL, 1);
    FT_BitmapGlyph bmp_stroke = (FT_BitmapGlyph)glyph_stroke;

    /* Use stroke bitmap dimensions (larger) */
    int w = (int)bmp_stroke->bitmap.width;
    int h = (int)bmp_stroke->bitmap.rows;

    if (w == 0 || h == 0) {
        /* Whitespace glyph */
        gi->width = 0;
        gi->height = 0;
        gi->bearing_x = 0;
        gi->bearing_y = 0;
        gi->advance = (int)(ctx->face->glyph->advance.x >> 6);
        gi->valid = true;
        FT_Done_Glyph(glyph_fill);
        FT_Done_Glyph(glyph_stroke);
        return true;
    }

    /* Check atlas space */
    if (ctx->cursor_x + w + GLYPH_PAD > ATLAS_W) {
        ctx->cursor_x = 0;
        ctx->cursor_y += ctx->row_height + GLYPH_PAD;
        ctx->row_height = 0;
    }
    if (ctx->cursor_y + h + GLYPH_PAD > ATLAS_H) {
        fprintf(stderr, "[Text] Atlas full! Cannot fit codepoint U+%04X\n", cp);
        gi->valid = false;
        FT_Done_Glyph(glyph_fill);
        FT_Done_Glyph(glyph_stroke);
        return false;
    }

    /* Write stroke (R) and fill (G) into atlas */
    int ox = ctx->cursor_x;
    int oy = ctx->cursor_y;

    /* Offsets: fill may be smaller and offset from stroke */
    int fill_ox = bmp_fill->left - bmp_stroke->left;
    int fill_oy = bmp_stroke->top - bmp_fill->top;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int atlas_idx = ((oy + row) * ATLAS_W + (ox + col)) * 2;

            /* Stroke channel (R) */
            uint8_t stroke_val = 0;
            if (row < (int)bmp_stroke->bitmap.rows && col < (int)bmp_stroke->bitmap.width) {
                stroke_val = bmp_stroke->bitmap.buffer[row * bmp_stroke->bitmap.pitch + col];
            }

            /* Fill channel (G) */
            uint8_t fill_val = 0;
            int fr = row - fill_oy;
            int fc = col - fill_ox;
            if (fr >= 0 && fr < (int)bmp_fill->bitmap.rows &&
                fc >= 0 && fc < (int)bmp_fill->bitmap.width) {
                fill_val = bmp_fill->bitmap.buffer[fr * bmp_fill->bitmap.pitch + fc];
            }

            ctx->atlas_data[atlas_idx + 0] = stroke_val;
            ctx->atlas_data[atlas_idx + 1] = fill_val;
        }
    }

    /* Upload to GPU */
    glBindTexture(GL_TEXTURE_2D, ctx->atlas_tex);
    /* Upload the region we just wrote */
    uint8_t *row_data = (uint8_t *)malloc(w * 2);
    for (int row = 0; row < h; row++) {
        int src_base = ((oy + row) * ATLAS_W + ox) * 2;
        memcpy(row_data, &ctx->atlas_data[src_base], w * 2);
        glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy + row, w, 1,
                        GL_RG, GL_UNSIGNED_BYTE, row_data);
    }
    free(row_data);

    /* Fill glyph info */
    gi->width = w;
    gi->height = h;
    gi->bearing_x = bmp_stroke->left;
    gi->bearing_y = bmp_stroke->top;
    gi->advance = (int)(ctx->face->glyph->advance.x >> 6);
    gi->u0 = (float)ox / ATLAS_W;
    gi->v0 = (float)oy / ATLAS_H;
    gi->u1 = (float)(ox + w) / ATLAS_W;
    gi->v1 = (float)(oy + h) / ATLAS_H;
    gi->valid = true;

    /* Advance cursor */
    ctx->cursor_x += w + GLYPH_PAD;
    if (h + GLYPH_PAD > ctx->row_height) ctx->row_height = h + GLYPH_PAD;

    FT_Done_Glyph(glyph_fill);
    FT_Done_Glyph(glyph_stroke);
    return true;
}

/* Pre-warm common characters */
static void prewarm(FontContext *ctx) {
    /* ASCII printable range */
    for (uint32_t c = 0x20; c < 0x7F; c++) {
        font_get_glyph(ctx, c);
    }

    /* Common CJK punctuation */
    static const uint32_t common[] = {
        0x3001, 0x3002, 0xFF01, 0xFF0C, 0xFF1A, 0xFF1F, /* punctuation */
        0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, /* quotes, dash, ellipsis */
        0x4E00, 0x4E86, 0x4E0D, 0x662F, 0x6211, 0x7684, /* 一了不是我的 */
        0x8FD9, 0x4ED6, 0x5979, 0x5B83, 0x4EEC, 0x4F60, /* 这他她它们你 */
        0x597D, 0x5927, 0x5C0F, 0x591A, 0x5C11, 0x8001, /* 好大小多少老 */
        0x6765, 0x53BB, 0x5230, 0x770B, 0x542C, 0x8BF4, /* 来去到看听说 */
        0x77E5, 0x9053, 0x60F3, 0x8981, 0x80FD, 0x4F1A, /* 知道想要能会 */
        0x53EF, 0x4EE5, 0x5F88, 0x90FD, 0x4E5F, 0x8FD8, /* 可以很都也还 */
        0x5C31, 0x8BF7, 0x8C22, 0x54C8, 0x55B5, 0x611F, /* 就请谢哈嘛感 */
        0x89C9, 0x559C, 0x6B22, 0x7231, 0x5FEB, 0x4E50, /* 觉喜欢爱快乐 */
        0x54ED, 0x7B11, 0x8D85, 0x7EA7, 0x68D2, 0x6253, /* 哭笑超级棒打 */
        0x53D1, 0x5F39, 0x5E55, 0x89C6, 0x9891, 0x64AD, /* 发弹幕视频播 */
        0x653E, 0x6682, 0x505C, 0x5F00, 0x59CB, 0x7ED3, /* 放暂停开始结 */
        0x675F, 0x97F3, 0x91CF, 0x5168, 0x5C4F, 0x8BBE, /* 束音量全屏设 */
        0x7F6E, 0x5173, 0x95ED, 0x52A0, 0x8F7D, 0x5B8C, /* 置关闭加载完 */
        0x6210, 0x5931, 0x8D25, 0x6210, 0x529F, 0       /* 成失败成功 */
    };
    for (int i = 0; common[i] != 0; i++) {
        font_get_glyph(ctx, common[i]);
    }
}

FontContext *font_ctx_create(const char *font_path, int base_size) {
    FontContext *ctx = (FontContext *)calloc(1, sizeof(FontContext));

    if (FT_Init_FreeType(&ctx->ft)) {
        fprintf(stderr, "[Text] FreeType init failed\n");
        free(ctx);
        return NULL;
    }

    /* Try user-provided font, then fall back to system fonts */
    const char *paths_to_try[] = {
        font_path,
        "assets/fonts/NotoSansSC-Regular.ttf",
        "assets/fonts/NotoSansCJK-Regular.ttc",
        "C:/Windows/Fonts/msyh.ttc",        /* Microsoft YaHei */
        "C:/Windows/Fonts/simsun.ttc",       /* SimSun */
        "C:/Windows/Fonts/simhei.ttf",       /* SimHei */
        "C:/Windows/Fonts/arial.ttf",        /* Fallback */
        NULL
    };

    bool loaded = false;
    for (int i = 0; paths_to_try[i]; i++) {
        if (!paths_to_try[i]) continue;
        if (FT_New_Face(ctx->ft, paths_to_try[i], 0, &ctx->face) == 0) {
            printf("[Text] Loaded font: %s\n", paths_to_try[i]);
            loaded = true;
            break;
        }
    }

    if (!loaded) {
        fprintf(stderr, "[Text] No suitable font found!\n");
        FT_Done_FreeType(ctx->ft);
        free(ctx);
        return NULL;
    }

    ctx->base_size = base_size;
    FT_Set_Pixel_Sizes(ctx->face, 0, base_size);
    ctx->line_height = (int)(ctx->face->size->metrics.height >> 6);

    /* Create stroker for outlines */
    FT_Stroker_New(ctx->ft, &ctx->stroker);
    FT_Stroker_Set(ctx->stroker, STROKE_RADIUS * 64, FT_STROKER_LINECAP_ROUND,
                   FT_STROKER_LINEJOIN_ROUND, 0);

    /* Create atlas texture (GL_RG8: R=stroke, G=fill) */
    ctx->atlas_data = (uint8_t *)calloc(ATLAS_W * ATLAS_H * 2, 1);

    glGenTextures(1, &ctx->atlas_tex);
    glBindTexture(GL_TEXTURE_2D, ctx->atlas_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, ATLAS_W, ATLAS_H, 0,
                 GL_RG, GL_UNSIGNED_BYTE, ctx->atlas_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Pre-warm common glyphs */
    prewarm(ctx);

    return ctx;
}

void font_ctx_destroy(FontContext *ctx) {
    if (!ctx) return;

    /* Free glyph map */
    for (int i = 0; i < GLYPH_MAP_SIZE; i++) {
        GlyphNode *n = ctx->map[i];
        while (n) {
            GlyphNode *next = n->next;
            free(n);
            n = next;
        }
    }

    if (ctx->atlas_tex) glDeleteTextures(1, &ctx->atlas_tex);
    free(ctx->atlas_data);
    if (ctx->stroker) FT_Stroker_Done(ctx->stroker);
    if (ctx->face) FT_Done_Face(ctx->face);
    if (ctx->ft) FT_Done_FreeType(ctx->ft);
    free(ctx);
}

const GlyphInfo *font_get_glyph(FontContext *ctx, uint32_t codepoint) {
    GlyphInfo *gi = find_glyph(ctx, codepoint);
    if (gi) return gi;

    /* Rasterize on demand */
    gi = insert_glyph(ctx, codepoint);
    rasterize_glyph(ctx, codepoint, gi);
    return gi;
}

GLuint font_get_atlas_texture(const FontContext *ctx) {
    return ctx->atlas_tex;
}

void font_get_atlas_size(const FontContext *ctx, int *w, int *h) {
    *w = ATLAS_W;
    *h = ATLAS_H;
}

float font_measure_text(FontContext *ctx, const char *text, float scale) {
    float width = 0;
    const char *p = text;
    uint32_t cp;
    while ((cp = utf8_decode(&p)) != 0) {
        const GlyphInfo *gi = font_get_glyph(ctx, cp);
        if (gi && gi->valid) {
            width += gi->advance * scale;
        }
    }
    return width;
}

int font_get_line_height(const FontContext *ctx) {
    return ctx->line_height;
}
