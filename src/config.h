/* config.h - Configuration persistence (JSON) */
#ifndef CONFIG_H
#define CONFIG_H

#include "i18n.h"
#include <stdbool.h>

typedef struct {
    /* Window */
    int window_x, window_y;
    int window_w, window_h;
    bool fullscreen;

    /* Playback */
    int volume;             /* 0..150 */
    double speed;           /* 0.25..4.0 */
    char last_file[512];
    double last_position;

    /* Danmaku */
    bool danmaku_enabled;
    float danmaku_opacity;  /* 0..1 */
    float danmaku_speed;    /* 0.5..2.0 */
    int danmaku_font_scale; /* 50..200 */

    /* App */
    Language language;
} AppConfig;

/* Load config from file. Returns default if file doesn't exist. */
void config_load(AppConfig *cfg);

/* Save config to file. */
void config_save(const AppConfig *cfg);

/* Get default config */
void config_defaults(AppConfig *cfg);

#endif /* CONFIG_H */
