/* danmaku.h - Danmaku display manager */
#ifndef DANMAKU_H
#define DANMAKU_H

#include "danmaku_parser.h"
#include <stdbool.h>

/* Runtime state for a visible danmaku */
typedef struct {
    const DanmakuItem *item;
    float x, y;           /* Current position */
    float width;           /* Measured text width */
    float speed;           /* Pixels per second for scroll */
    int   track;           /* Assigned track index */
    bool  active;
} ActiveDanmaku;

/* Danmaku manager */
typedef struct {
    DanmakuList    list;         /* Parsed items (sorted by time) */
    ActiveDanmaku *active;       /* Currently visible pool */
    int            active_count;
    int            active_cap;

    int    next_index;           /* Next item index to activate */
    double last_time;            /* Last playback time */
    bool   enabled;              /* Danmaku on/off toggle */
    float  opacity;              /* 0..1 */
    float  speed_factor;         /* Speed multiplier */
    int    font_scale;           /* Font size scale percentage (100=normal) */
    int    max_tracks;           /* Number of danmaku tracks */

    /* Track occupation (for anti-overlap) */
    float  *scroll_track_end;    /* When each scroll track will be free (x position) */
    float  *top_track_until;     /* When each top track will be free (time) */
    float  *bottom_track_until;  /* When each bottom track will be free (time) */
} DanmakuManager;

/* Initialize with screen dimensions */
void danmaku_mgr_init(DanmakuManager *mgr, int screen_w, int screen_h);
void danmaku_mgr_free(DanmakuManager *mgr);

/* Load danmaku from file */
bool danmaku_mgr_load(DanmakuManager *mgr, const char *path);

/* Update: activate new danmaku and move existing ones */
void danmaku_mgr_update(DanmakuManager *mgr, double time, float dt,
                         int screen_w, int screen_h, bool paused);

/* Reset state (e.g., after seek) */
void danmaku_mgr_reset(DanmakuManager *mgr, double time);

/* Get active items for rendering */
const ActiveDanmaku *danmaku_mgr_get_active(const DanmakuManager *mgr, int *count);

#endif /* DANMAKU_H */
