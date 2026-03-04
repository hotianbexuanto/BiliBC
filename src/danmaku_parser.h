/* danmaku_parser.h - Parse XML/JSON/ASS danmaku files */
#ifndef DANMAKU_PARSER_H
#define DANMAKU_PARSER_H

#include <stdint.h>
#include <stddef.h>

/* Danmaku display mode */
typedef enum {
    DANMAKU_MODE_SCROLL  = 1,  /* Right to left scroll */
    DANMAKU_MODE_BOTTOM  = 4,  /* Fixed bottom */
    DANMAKU_MODE_TOP     = 5,  /* Fixed top */
} DanmakuMode;

/* Single danmaku item */
typedef struct {
    double   time;       /* Appear time in seconds */
    int      mode;       /* DanmakuMode */
    int      fontsize;   /* Font size (25=normal, 36=large) */
    uint32_t color;      /* RGB color (0xRRGGBB) */
    char     text[256];  /* UTF-8 text content */
} DanmakuItem;

/* Parsed danmaku list */
typedef struct {
    DanmakuItem *items;
    size_t       count;
    size_t       capacity;
} DanmakuList;

/* Initialize a danmaku list */
void danmaku_list_init(DanmakuList *list);

/* Free a danmaku list */
void danmaku_list_free(DanmakuList *list);

/* Sort items by time */
void danmaku_list_sort(DanmakuList *list);

/* Parse from file (auto-detect format by extension) */
int danmaku_parse_file(const char *path, DanmakuList *out);

/* Parse from memory with explicit format */
int danmaku_parse_xml(const char *data, size_t len, DanmakuList *out);
int danmaku_parse_json(const char *data, size_t len, DanmakuList *out);
int danmaku_parse_ass(const char *data, size_t len, DanmakuList *out);

#endif /* DANMAKU_PARSER_H */
