/* danmaku_parser.c - Parse XML/JSON/ASS danmaku files */
#include "danmaku_parser.h"
#include <yxml.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Portable case-insensitive compare */
#ifdef _MSC_VER
#define strcasecmp strcasecmp
#else
#include <strings.h>
#endif

/* --- Internal helpers --- */

static void list_ensure(DanmakuList *list, size_t need) {
    if (list->count + need <= list->capacity) return;
    size_t newcap = list->capacity ? list->capacity * 2 : 128;
    while (newcap < list->count + need) newcap *= 2;
    list->items = (DanmakuItem *)realloc(list->items, newcap * sizeof(DanmakuItem));
    list->capacity = newcap;
}

static DanmakuItem *list_add(DanmakuList *list) {
    list_ensure(list, 1);
    DanmakuItem *item = &list->items[list->count++];
    memset(item, 0, sizeof(*item));
    item->mode = DANMAKU_MODE_SCROLL;
    item->fontsize = 25;
    item->color = 0xFFFFFF;
    return item;
}

static int cmp_by_time(const void *a, const void *b) {
    const DanmakuItem *da = (const DanmakuItem *)a;
    const DanmakuItem *db = (const DanmakuItem *)b;
    if (da->time < db->time) return -1;
    if (da->time > db->time) return 1;
    return 0;
}

static char *read_entire_file(const char *path, size_t *out_len) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    fread(buf, 1, sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    if (out_len) *out_len = (size_t)sz;
    return buf;
}

static const char *get_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

/* --- Public API --- */

void danmaku_list_init(DanmakuList *list) {
    memset(list, 0, sizeof(*list));
}

void danmaku_list_free(DanmakuList *list) {
    free(list->items);
    memset(list, 0, sizeof(*list));
}

void danmaku_list_sort(DanmakuList *list) {
    if (list->count > 1) {
        qsort(list->items, list->count, sizeof(DanmakuItem), cmp_by_time);
    }
}

/* --- XML parser (Bilibili XML format) --- */
/* Format: <d p="time,mode,fontsize,color,...">text</d> */

int danmaku_parse_xml(const char *data, size_t len, DanmakuList *out) {
    char stack[4096];
    yxml_t xml;
    yxml_init(&xml, stack, sizeof(stack));

    int in_d = 0;           /* inside <d> element */
    int in_p = 0;           /* inside p= attribute */
    char p_val[256] = {0};  /* p attribute value */
    int p_len = 0;
    char content[256] = {0};
    int c_len = 0;

    for (size_t i = 0; i < len; i++) {
        yxml_ret_t r = yxml_parse(&xml, data[i]);
        if (r < 0) continue; /* skip errors */

        switch (r) {
        case YXML_ELEMSTART:
            if (xml.elem && strcmp(xml.elem, "d") == 0) {
                in_d = 1;
                p_len = 0; p_val[0] = 0;
                c_len = 0; content[0] = 0;
            }
            break;

        case YXML_ATTRSTART:
            if (in_d && xml.attr && strcmp(xml.attr, "p") == 0) {
                in_p = 1;
                p_len = 0; p_val[0] = 0;
            }
            break;

        case YXML_ATTRVAL:
            if (in_p && p_len < (int)sizeof(p_val) - 2) {
                p_val[p_len++] = xml.data[0];
                p_val[p_len] = 0;
            }
            break;

        case YXML_ATTREND:
            in_p = 0;
            break;

        case YXML_CONTENT:
            if (in_d && c_len < (int)sizeof(content) - 2) {
                content[c_len++] = xml.data[0];
                content[c_len] = 0;
            }
            break;

        case YXML_ELEMEND:
            if (in_d) {
                /* Parse p="time,mode,fontsize,color,..." */
                double time = 0;
                int mode = 1, fontsize = 25;
                unsigned int color = 0xFFFFFF;

                char *tok = p_val;
                char *comma;
                int field = 0;
                while (tok && *tok) {
                    comma = strchr(tok, ',');
                    if (comma) *comma = 0;

                    switch (field) {
                    case 0: time = atof(tok); break;
                    case 1: mode = atoi(tok); break;
                    case 2: fontsize = atoi(tok); break;
                    case 3: color = (unsigned int)strtoul(tok, NULL, 10); break;
                    }
                    field++;
                    tok = comma ? comma + 1 : NULL;
                }

                if (c_len > 0) {
                    DanmakuItem *item = list_add(out);
                    item->time = time;
                    item->mode = mode;
                    item->fontsize = fontsize;
                    item->color = color & 0xFFFFFF;
                    strncpy(item->text, content, sizeof(item->text) - 1);
                }
                in_d = 0;
            }
            break;

        default:
            break;
        }
    }

    return (int)out->count;
}

/* --- JSON parser (Bilibili JSON format) --- */
/* Format: [{ "progress": ms, "mode": 1, "fontsize": 25, "color": 16777215, "content": "text" }] */

int danmaku_parse_json(const char *data, size_t len, DanmakuList *out) {
    (void)len;
    cJSON *root = cJSON_Parse(data);
    if (!root) return -1;

    /* Handle both array and object with array field */
    cJSON *arr = root;
    if (!cJSON_IsArray(root)) {
        /* Try common wrapper fields */
        arr = cJSON_GetObjectItem(root, "data");
        if (!arr) arr = cJSON_GetObjectItem(root, "elems");
        if (!arr || !cJSON_IsArray(arr)) {
            cJSON_Delete(root);
            return -1;
        }
    }

    cJSON *item_json;
    cJSON_ArrayForEach(item_json, arr) {
        DanmakuItem *item = list_add(out);

        cJSON *progress = cJSON_GetObjectItem(item_json, "progress");
        if (progress && cJSON_IsNumber(progress)) {
            item->time = progress->valuedouble / 1000.0;  /* ms → s */
        }

        cJSON *mode = cJSON_GetObjectItem(item_json, "mode");
        if (mode && cJSON_IsNumber(mode)) {
            item->mode = mode->valueint;
        }

        cJSON *fontsize = cJSON_GetObjectItem(item_json, "fontsize");
        if (fontsize && cJSON_IsNumber(fontsize)) {
            item->fontsize = fontsize->valueint;
        }

        cJSON *color = cJSON_GetObjectItem(item_json, "color");
        if (color && cJSON_IsNumber(color)) {
            item->color = (uint32_t)color->valuedouble & 0xFFFFFF;
        }

        cJSON *content = cJSON_GetObjectItem(item_json, "content");
        if (content && cJSON_IsString(content)) {
            strncpy(item->text, content->valuestring, sizeof(item->text) - 1);
        }
    }

    cJSON_Delete(root);
    return (int)out->count;
}

/* --- ASS parser (Bilibili Evolved export format) --- */
/* Dialogue: 0,H:MM:SS.cc,H:MM:SS.cc,Style,,0,0,0,,{\move(x1,y1,x2,y2)...\c&HBBGGRR&}text */

static double parse_ass_time(const char *s) {
    int h = 0, m = 0, sec = 0, cs = 0;
    if (sscanf(s, "%d:%d:%d.%d", &h, &m, &sec, &cs) >= 3) {
        return h * 3600.0 + m * 60.0 + sec + cs / 100.0;
    }
    return 0.0;
}

static uint32_t parse_ass_color(const char *s) {
    /* ASS color: \c&HBBGGRR& → we need 0xRRGGBB */
    unsigned int bgr = 0;
    if (sscanf(s, "%x", &bgr) == 1) {
        unsigned int r = bgr & 0xFF;
        unsigned int g = (bgr >> 8) & 0xFF;
        unsigned int b = (bgr >> 16) & 0xFF;
        return (r << 16) | (g << 8) | b;
    }
    return 0xFFFFFF;
}

int danmaku_parse_ass(const char *data, size_t len, DanmakuList *out) {
    const char *line = data;
    const char *end = data + len;

    while (line < end) {
        /* Find end of line */
        const char *eol = line;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;

        /* Check for Dialogue: line */
        if (eol - line > 10 && strncmp(line, "Dialogue:", 9) == 0) {
            /* Parse comma-separated fields */
            char buf[1024];
            size_t llen = (size_t)(eol - line);
            if (llen >= sizeof(buf)) llen = sizeof(buf) - 1;
            memcpy(buf, line, llen);
            buf[llen] = 0;

            char *p = buf + 9; /* skip "Dialogue:" */
            while (*p == ' ') p++;

            /* Fields: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text */
            char *fields[10] = {0};
            int fi = 0;
            char *tok = p;
            for (char *c = p; *c && fi < 9; c++) {
                if (*c == ',') {
                    *c = 0;
                    fields[fi++] = tok;
                    tok = c + 1;
                }
            }
            fields[fi] = tok; /* last field = Text */

            if (fi < 9) goto next_line;

            double start_time = parse_ass_time(fields[1]);
            char *text_field = fields[9];

            /* Extract color from override tags: \c&HBBGGRR& */
            uint32_t color = 0xFFFFFF;
            char *ctag = strstr(text_field, "\\c&H");
            if (ctag) {
                color = parse_ass_color(ctag + 4);
            }

            /* Determine mode from \move or \an tags */
            int mode = DANMAKU_MODE_SCROLL;
            if (strstr(text_field, "\\move(")) {
                mode = DANMAKU_MODE_SCROLL;
            }
            if (strstr(text_field, "\\an8")) {
                mode = DANMAKU_MODE_TOP;
            }
            if (strstr(text_field, "\\an2")) {
                mode = DANMAKU_MODE_BOTTOM;
            }

            /* Strip override tags: find last '}' */
            char *actual_text = text_field;
            char *rbrace = strrchr(text_field, '}');
            if (rbrace) actual_text = rbrace + 1;

            /* Determine fontsize from style name */
            int fontsize = 25;
            if (fields[3]) {
                if (strstr(fields[3], "Large") || strstr(fields[3], "large"))
                    fontsize = 36;
                else if (strstr(fields[3], "Small") || strstr(fields[3], "small"))
                    fontsize = 18;
            }

            if (actual_text && *actual_text) {
                DanmakuItem *item = list_add(out);
                item->time = start_time;
                item->mode = mode;
                item->fontsize = fontsize;
                item->color = color;
                strncpy(item->text, actual_text, sizeof(item->text) - 1);
            }
        }

    next_line:
        line = eol;
        while (line < end && (*line == '\n' || *line == '\r')) line++;
    }

    return (int)out->count;
}

/* --- Auto-detect and parse --- */

int danmaku_parse_file(const char *path, DanmakuList *out) {
    size_t len = 0;
    char *data = read_entire_file(path, &len);
    if (!data) return -1;

    const char *ext = get_extension(path);
    int result = -1;

    if (strcasecmp(ext, "xml") == 0) {
        result = danmaku_parse_xml(data, len, out);
    } else if (strcasecmp(ext, "json") == 0) {
        result = danmaku_parse_json(data, len, out);
    } else if (strcasecmp(ext, "ass") == 0) {
        result = danmaku_parse_ass(data, len, out);
    } else {
        /* Try to auto-detect */
        if (data[0] == '<' || (len > 3 && (unsigned char)data[0] == 0xEF)) {
            result = danmaku_parse_xml(data, len, out);
        } else if (data[0] == '[' || data[0] == '{') {
            result = danmaku_parse_json(data, len, out);
        } else if (strstr(data, "[Script Info]")) {
            result = danmaku_parse_ass(data, len, out);
        }
    }

    if (result > 0) {
        danmaku_list_sort(out);
    }

    free(data);
    return result;
}
