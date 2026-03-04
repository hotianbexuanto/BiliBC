/* i18n.c - Internationalization (Chinese/English) */
#include "i18n.h"
#include <string.h>

typedef struct {
    const char *key;
    const char *values[LANG_COUNT]; /* [EN, ZH] */
} I18nEntry;

static Language current_lang = LANG_EN;

/* String table */
static const I18nEntry strings[] = {
    /* App */
    { "app.title",       { "BiliBC Player",              "BiliBC \xe6\x92\xad\xe6\x94\xbe\xe5\x99\xa8" } },
    { "app.drop_hint",   { "Drop video file here or press O to open",
                           "\xe6\x8b\x96\xe6\x94\xbe\xe8\xa7\x86\xe9\xa2\x91\xe6\x96\x87\xe4\xbb\xb6\xe6\x88\x96\xe6\x8c\x89 O \xe6\x89\x93\xe5\xbc\x80" } },

    /* Controls */
    { "ctrl.play",       { "Play",                        "\xe6\x92\xad\xe6\x94\xbe" } },
    { "ctrl.pause",      { "Pause",                       "\xe6\x9a\x82\xe5\x81\x9c" } },
    { "ctrl.volume",     { "Volume",                      "\xe9\x9f\xb3\xe9\x87\x8f" } },
    { "ctrl.fullscreen", { "Fullscreen",                  "\xe5\x85\xa8\xe5\xb1\x8f" } },
    { "ctrl.danmaku",    { "Danmaku",                     "\xe5\xbc\xb9\xe5\xb9\x95" } },
    { "ctrl.speed",      { "Speed",                       "\xe5\x80\x8d\xe9\x80\x9f" } },
    { "ctrl.screenshot", { "Screenshot",                  "\xe6\x88\xaa\xe5\x9b\xbe" } },

    /* Menu */
    { "menu.open_video",   { "Open Video",                "\xe6\x89\x93\xe5\xbc\x80\xe8\xa7\x86\xe9\xa2\x91" } },
    { "menu.open_danmaku", { "Open Danmaku",              "\xe6\x89\x93\xe5\xbc\x80\xe5\xbc\xb9\xe5\xb9\x95" } },
    { "menu.open_url",     { "Open URL",                  "\xe6\x89\x93\xe5\xbc\x80\xe7\xbd\x91\xe5\x9d\x80" } },
    { "menu.language",     { "Language",                   "\xe8\xaf\xad\xe8\xa8\x80" } },

    /* Messages */
    { "msg.loading",     { "Loading...",                   "\xe5\x8a\xa0\xe8\xbd\xbd\xe4\xb8\xad..." } },
    { "msg.no_video",    { "No video loaded",             "\xe6\x9c\xaa\xe5\x8a\xa0\xe8\xbd\xbd\xe8\xa7\x86\xe9\xa2\x91" } },
    { "msg.danmaku_loaded", { "Danmaku loaded: %d items", "\xe5\xbc\xb9\xe5\xb9\x95\xe5\xb7\xb2\xe5\x8a\xa0\xe8\xbd\xbd: %d \xe6\x9d\xa1" } },
    { "msg.screenshot_saved", { "Screenshot saved",       "\xe6\x88\xaa\xe5\x9b\xbe\xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98" } },
    { "msg.url_hint",    { "Paste video URL and press Enter",
                           "\xe7\xb2\x98\xe8\xb4\xb4\xe8\xa7\x86\xe9\xa2\x91\xe7\xbd\x91\xe5\x9d\x80\xe5\xb9\xb6\xe6\x8c\x89\xe5\x9b\x9e\xe8\xbd\xa6" } },

    /* Speed options */
    { "speed.0.25",      { "0.25x",                       "0.25x" } },
    { "speed.0.5",       { "0.5x",                        "0.5x" } },
    { "speed.1.0",       { "1.0x",                        "1.0x" } },
    { "speed.1.5",       { "1.5x",                        "1.5x" } },
    { "speed.2.0",       { "2.0x",                        "2.0x" } },

    { NULL, { NULL, NULL } }  /* sentinel */
};

void i18n_init(Language lang) {
    current_lang = lang;
}

void i18n_set_language(Language lang) {
    if (lang >= 0 && lang < LANG_COUNT) {
        current_lang = lang;
    }
}

Language i18n_get_language(void) {
    return current_lang;
}

const char *i18n_get(const char *key) {
    for (int i = 0; strings[i].key != NULL; i++) {
        if (strcmp(strings[i].key, key) == 0) {
            const char *val = strings[i].values[current_lang];
            return val ? val : key;
        }
    }
    return key;
}
