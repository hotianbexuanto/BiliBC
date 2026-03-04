/* player.c - libmpv video playback wrapper */
#include "player.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Player {
    mpv_handle *mpv;
    mpv_render_context *render;
    bool want_redraw;
    char title_buf[512];
};

static void on_mpv_redraw(void *ctx) {
    Player *p = (Player *)ctx;
    p->want_redraw = true;
}

Player *player_create(void) {
    Player *p = (Player *)calloc(1, sizeof(Player));
    if (!p) return NULL;

    p->mpv = mpv_create();
    if (!p->mpv) { free(p); return NULL; }

    /* Set default options */
    mpv_set_option_string(p->mpv, "vo", "libmpv");
    mpv_set_option_string(p->mpv, "hwdec", "auto");
    mpv_set_option_string(p->mpv, "keep-open", "yes");
    mpv_set_option_string(p->mpv, "idle", "yes");
    mpv_set_option_string(p->mpv, "input-default-bindings", "no");
    mpv_set_option_string(p->mpv, "osc", "no");
    mpv_set_option_string(p->mpv, "terminal", "no");

    if (mpv_initialize(p->mpv) < 0) {
        fprintf(stderr, "[Player] mpv_initialize failed\n");
        mpv_destroy(p->mpv);
        free(p);
        return NULL;
    }

    mpv_observe_property(p->mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(p->mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(p->mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(p->mpv, 0, "media-title", MPV_FORMAT_STRING);

    return p;
}

void player_destroy(Player *p) {
    if (!p) return;
    if (p->render) {
        mpv_render_context_set_update_callback(p->render, NULL, NULL);
        mpv_render_context_free(p->render);
    }
    if (p->mpv) {
        mpv_terminate_destroy(p->mpv);
    }
    free(p);
}

bool player_init_render(Player *p, void *(*get_proc)(void *ctx, const char *name), void *ctx) {
    mpv_opengl_init_params gl_init = {
        .get_proc_address = (void *(*)(void *, const char *))get_proc,
        .get_proc_address_ctx = ctx,
    };
    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL },
        { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init },
        { 0 }
    };

    int r = mpv_render_context_create(&p->render, p->mpv, params);
    if (r < 0) {
        fprintf(stderr, "[Player] render context create failed: %s\n", mpv_error_string(r));
        return false;
    }

    mpv_render_context_set_update_callback(p->render, on_mpv_redraw, p);
    return true;
}

bool player_load(Player *p, const char *path) {
    const char *cmd[] = { "loadfile", path, NULL };
    int r = mpv_command(p->mpv, cmd);
    if (r < 0) {
        fprintf(stderr, "[Player] loadfile failed: %s\n", mpv_error_string(r));
        return false;
    }
    return true;
}

void player_render(Player *p, GLuint fbo, int width, int height) {
    if (!p->render) return;

    mpv_opengl_fbo mpv_fbo = {
        .fbo = (int)fbo,
        .w = width,
        .h = height,
        .internal_format = 0,
    };
    int flip_y = 1;

    mpv_render_param params[] = {
        { MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo },
        { MPV_RENDER_PARAM_FLIP_Y, &flip_y },
        { 0 }
    };

    mpv_render_context_render(p->render, params);
    p->want_redraw = false;
}

void player_report_flip(Player *p) {
    if (p->render) {
        mpv_render_context_report_swap(p->render);
    }
}

void player_toggle_pause(Player *p) {
    const char *cmd[] = { "cycle", "pause", NULL };
    mpv_command_async(p->mpv, 0, cmd);
}

void player_seek(Player *p, double seconds) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f", seconds);
    const char *cmd[] = { "seek", buf, "relative", NULL };
    mpv_command_async(p->mpv, 0, cmd);
}

void player_set_volume(Player *p, int volume) {
    if (volume < 0) volume = 0;
    if (volume > 150) volume = 150;
    int64_t v = volume;
    mpv_set_property(p->mpv, "volume", MPV_FORMAT_INT64, &v);
}

int player_get_volume(Player *p) {
    int64_t v = 100;
    mpv_get_property(p->mpv, "volume", MPV_FORMAT_INT64, &v);
    return (int)v;
}

void player_set_speed(Player *p, double speed) {
    mpv_set_property(p->mpv, "speed", MPV_FORMAT_DOUBLE, &speed);
}

double player_get_speed(Player *p) {
    double s = 1.0;
    mpv_get_property(p->mpv, "speed", MPV_FORMAT_DOUBLE, &s);
    return s;
}

double player_get_time(Player *p) {
    double t = 0.0;
    mpv_get_property(p->mpv, "time-pos", MPV_FORMAT_DOUBLE, &t);
    return t;
}

double player_get_duration(Player *p) {
    double d = 0.0;
    mpv_get_property(p->mpv, "duration", MPV_FORMAT_DOUBLE, &d);
    return d;
}

bool player_is_paused(Player *p) {
    int f = 0;
    mpv_get_property(p->mpv, "pause", MPV_FORMAT_FLAG, &f);
    return f != 0;
}

bool player_is_idle(Player *p) {
    int f = 0;
    mpv_get_property(p->mpv, "idle-active", MPV_FORMAT_FLAG, &f);
    return f != 0;
}

const char *player_get_title(Player *p) {
    char *title = NULL;
    if (mpv_get_property(p->mpv, "media-title", MPV_FORMAT_STRING, &title) == 0 && title) {
        snprintf(p->title_buf, sizeof(p->title_buf), "%s", title);
        mpv_free(title);
        return p->title_buf;
    }
    return "BiliBC Player";
}

bool player_process_events(Player *p) {
    bool need_redraw = p->want_redraw;
    while (1) {
        mpv_event *ev = mpv_wait_event(p->mpv, 0);
        if (ev->event_id == MPV_EVENT_NONE) break;

        switch (ev->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE:
            need_redraw = true;
            break;
        case MPV_EVENT_FILE_LOADED:
            need_redraw = true;
            break;
        case MPV_EVENT_END_FILE:
            break;
        default:
            break;
        }
    }
    return need_redraw;
}

void player_screenshot(Player *p, const char *filename) {
    const char *cmd[] = { "screenshot-to-file", filename, "video", NULL };
    mpv_command_async(p->mpv, 0, cmd);
}
