/* danmaku.c - Danmaku display manager */
#include "danmaku.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_ACTIVE 512
#define TRACK_HEIGHT 40
#define SCROLL_DURATION 6.0f   /* seconds a scroll danmaku takes to cross screen */
#define FIXED_DURATION  4.0f   /* seconds a top/bottom danmaku stays */

void danmaku_mgr_init(DanmakuManager *mgr, int screen_w, int screen_h) {
    memset(mgr, 0, sizeof(*mgr));
    danmaku_list_init(&mgr->list);

    mgr->active_cap = MAX_ACTIVE;
    mgr->active = (ActiveDanmaku *)calloc(MAX_ACTIVE, sizeof(ActiveDanmaku));
    mgr->enabled = true;
    mgr->opacity = 1.0f;
    mgr->speed_factor = 1.0f;
    mgr->font_scale = 100;

    /* Calculate track count based on screen height (use 75% of height) */
    mgr->max_tracks = (int)(screen_h * 0.75f / TRACK_HEIGHT);
    if (mgr->max_tracks < 1) mgr->max_tracks = 1;
    if (mgr->max_tracks > 30) mgr->max_tracks = 30;

    mgr->scroll_track_end = (float *)calloc(mgr->max_tracks, sizeof(float));
    mgr->top_track_until = (float *)calloc(mgr->max_tracks, sizeof(float));
    mgr->bottom_track_until = (float *)calloc(mgr->max_tracks, sizeof(float));

    (void)screen_w;
}

void danmaku_mgr_free(DanmakuManager *mgr) {
    danmaku_list_free(&mgr->list);
    free(mgr->active);
    free(mgr->scroll_track_end);
    free(mgr->top_track_until);
    free(mgr->bottom_track_until);
    memset(mgr, 0, sizeof(*mgr));
}

bool danmaku_mgr_load(DanmakuManager *mgr, const char *path) {
    danmaku_list_free(&mgr->list);
    danmaku_list_init(&mgr->list);
    mgr->next_index = 0;
    mgr->active_count = 0;

    int count = danmaku_parse_file(path, &mgr->list);
    if (count < 0) return false;

    printf("[Danmaku] Loaded %d items from %s\n", count, path);
    return true;
}

/* Estimate text width in pixels (rough: ~fontsize * 0.6 per char) */
static float estimate_width(const char *text, int fontsize) {
    int chars = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p < 0x80) { p++; chars++; }
        else if ((*p & 0xE0) == 0xC0) { p += 2; chars++; }
        else if ((*p & 0xF0) == 0xE0) { p += 3; chars++; }
        else if ((*p & 0xF8) == 0xF0) { p += 4; chars++; }
        else { p++; chars++; }
    }
    /* CJK chars are roughly square, Latin ~0.6 width */
    return (float)chars * fontsize * 0.65f;
}

static int find_scroll_track(DanmakuManager *mgr, float width, float screen_w, double time) {
    float speed = (screen_w + width) / (SCROLL_DURATION / mgr->speed_factor);

    for (int i = 0; i < mgr->max_tracks; i++) {
        /* Track is free if the previous danmaku's tail has cleared enough */
        float end_x = mgr->scroll_track_end[i];
        if (end_x <= screen_w * 0.85f) {
            /* Assign this track */
            mgr->scroll_track_end[i] = screen_w + width;
            return i;
        }
    }
    return -1; /* No free track, drop this danmaku */
}

static int find_top_track(DanmakuManager *mgr, double time) {
    for (int i = 0; i < mgr->max_tracks; i++) {
        if (mgr->top_track_until[i] <= (float)time) {
            mgr->top_track_until[i] = (float)(time + FIXED_DURATION);
            return i;
        }
    }
    return -1;
}

static int find_bottom_track(DanmakuManager *mgr, double time, int screen_h) {
    for (int i = 0; i < mgr->max_tracks; i++) {
        if (mgr->bottom_track_until[i] <= (float)time) {
            mgr->bottom_track_until[i] = (float)(time + FIXED_DURATION);
            return i;
        }
    }
    (void)screen_h;
    return -1;
}

void danmaku_mgr_update(DanmakuManager *mgr, double time, float dt,
                         int screen_w, int screen_h)
{
    if (!mgr->enabled) return;

    /* Update scroll track end positions */
    for (int i = 0; i < mgr->max_tracks; i++) {
        float speed = (screen_w + 200.0f) / (SCROLL_DURATION / mgr->speed_factor);
        mgr->scroll_track_end[i] -= speed * dt;
        if (mgr->scroll_track_end[i] < 0) mgr->scroll_track_end[i] = 0;
    }

    /* Remove expired active danmaku */
    for (int i = mgr->active_count - 1; i >= 0; i--) {
        ActiveDanmaku *ad = &mgr->active[i];
        if (!ad->active) continue;

        bool expired = false;
        if (ad->item->mode == DANMAKU_MODE_SCROLL) {
            expired = (ad->x + ad->width < 0);
        } else {
            float duration = FIXED_DURATION;
            expired = (time > ad->item->time + duration);
        }

        if (expired) {
            ad->active = false;
            /* Compact: swap with last */
            if (i < mgr->active_count - 1) {
                mgr->active[i] = mgr->active[mgr->active_count - 1];
            }
            mgr->active_count--;
        }
    }

    /* Activate new danmaku */
    while (mgr->next_index < (int)mgr->list.count) {
        const DanmakuItem *item = &mgr->list.items[mgr->next_index];
        if (item->time > time) break; /* Not yet time */
        if (item->time < time - 0.5) {
            /* Too old, skip */
            mgr->next_index++;
            continue;
        }
        if (mgr->active_count >= mgr->active_cap) break; /* Pool full */

        float width = estimate_width(item->text, item->fontsize);
        int track = -1;

        switch (item->mode) {
        case DANMAKU_MODE_SCROLL:
        default:
            track = find_scroll_track(mgr, width, (float)screen_w, time);
            break;
        case DANMAKU_MODE_TOP:
            track = find_top_track(mgr, time);
            break;
        case DANMAKU_MODE_BOTTOM:
            track = find_bottom_track(mgr, time, screen_h);
            break;
        }

        if (track >= 0) {
            ActiveDanmaku *ad = &mgr->active[mgr->active_count++];
            ad->item = item;
            ad->width = width;
            ad->track = track;
            ad->active = true;

            float speed_mult = mgr->speed_factor;
            ad->speed = ((float)screen_w + width) / (SCROLL_DURATION / speed_mult);

            switch (item->mode) {
            case DANMAKU_MODE_SCROLL:
            default:
                ad->x = (float)screen_w;
                ad->y = (float)(track * TRACK_HEIGHT + 10);
                break;
            case DANMAKU_MODE_TOP:
                ad->x = ((float)screen_w - width) * 0.5f;
                ad->y = (float)(track * TRACK_HEIGHT + 10);
                break;
            case DANMAKU_MODE_BOTTOM:
                ad->x = ((float)screen_w - width) * 0.5f;
                ad->y = (float)(screen_h - (track + 1) * TRACK_HEIGHT - 60);
                break;
            }
        }

        mgr->next_index++;
    }

    /* Move active scroll danmaku */
    for (int i = 0; i < mgr->active_count; i++) {
        ActiveDanmaku *ad = &mgr->active[i];
        if (!ad->active) continue;
        if (ad->item->mode == DANMAKU_MODE_SCROLL) {
            ad->x -= ad->speed * dt;
        }
    }

    mgr->last_time = time;
}

void danmaku_mgr_reset(DanmakuManager *mgr, double time) {
    mgr->active_count = 0;

    /* Binary search for the correct index */
    int lo = 0, hi = (int)mgr->list.count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (mgr->list.items[mid].time < time)
            lo = mid + 1;
        else
            hi = mid;
    }
    mgr->next_index = lo;

    /* Clear track state */
    for (int i = 0; i < mgr->max_tracks; i++) {
        mgr->scroll_track_end[i] = 0;
        mgr->top_track_until[i] = 0;
        mgr->bottom_track_until[i] = 0;
    }

    mgr->last_time = time;
}

const ActiveDanmaku *danmaku_mgr_get_active(const DanmakuManager *mgr, int *count) {
    *count = mgr->active_count;
    return mgr->active;
}
