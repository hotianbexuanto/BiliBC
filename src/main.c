/* main.c - BiliBC: Bilibili-style danmaku video player
 *
 * Entry point, SDL2/OpenGL initialization, main loop,
 * input handling, compositing pipeline.
 */
#include <SDL.h>
#include <glad/glad.h>

#include "player.h"
#include "shader.h"
#include "danmaku.h"
#include "danmaku_parser.h"
#include "text.h"
#include "text_renderer.h"
#include "blur.h"
#include "ui.h"
#include "i18n.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Portable case-insensitive compare */
#ifdef _MSC_VER
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* Platform functions (from platform_win32.c) */
extern char *platform_open_file_dialog(const char *title, const char *filter);
extern char *platform_open_danmaku_dialog(void);
extern char *platform_save_file_dialog(const char *default_name);
extern char *platform_get_clipboard_text(void);

/* STB image write for screenshots (embedded) */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

/* ------------------------------------------------------------------ */
/* Application state                                                   */
/* ------------------------------------------------------------------ */
typedef struct {
    SDL_Window   *window;
    SDL_GLContext  gl_ctx;
    Player       *player;
    FontContext   *font;
    TextRenderer *text_renderer;
    BlurPipeline *blur;
    UIContext      ui;
    DanmakuManager danmaku;
    AppConfig      config;

    /* Rendering */
    GLuint video_fbo, video_tex;       /* Video render target */
    GLuint scene_fbo, scene_tex;       /* Video + danmaku */
    GLuint composite_prog;
    GLuint quad_vao, quad_vbo;
    GLuint quad_prog;                  /* Simple textured quad */

    int width, height;
    bool running;
    bool fullscreen;

    /* Input state */
    int  last_mouse_x, last_mouse_y;
    bool mouse_moved;

    /* URL input mode */
    bool  url_input_active;
    char  url_buf[2048];
    int   url_len;

    /* Speed menu */
    bool speed_menu_open;

    /* Timing */
    Uint64 last_tick;
    double prev_player_time;  /* For detecting seeks */
} App;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */
static bool app_init(App *app, int argc, char **argv);
static void app_shutdown(App *app);
static void app_handle_event(App *app, SDL_Event *e);
static void app_update(App *app, float dt);
static void app_render(App *app);
static void app_create_fbos(App *app);
static void app_destroy_fbos(App *app);
static void create_fullscreen_quad(GLuint *vao, GLuint *vbo);
static void draw_fullscreen_quad(GLuint vao);
static void take_screenshot_with_danmaku(App *app);
static void *gl_get_proc(void *ctx, const char *name);
static void open_video(App *app, const char *path);
static void open_danmaku(App *app, const char *path);
static void try_auto_load_danmaku(App *app, const char *video_path);
static void format_time(double seconds, char *buf, size_t buf_size);

/* ------------------------------------------------------------------ */
/* OpenGL proc address helper                                          */
/* ------------------------------------------------------------------ */
static void *gl_get_proc(void *ctx, const char *name) {
    (void)ctx;
    return (void *)SDL_GL_GetProcAddress(name);
}

/* ------------------------------------------------------------------ */
/* FBO management                                                      */
/* ------------------------------------------------------------------ */
static void create_fbo_pair(GLuint *fbo, GLuint *tex, int w, int h) {
    glGenFramebuffers(1, fbo);
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void app_create_fbos(App *app) {
    create_fbo_pair(&app->video_fbo, &app->video_tex, app->width, app->height);
    create_fbo_pair(&app->scene_fbo, &app->scene_tex, app->width, app->height);
}

static void app_destroy_fbos(App *app) {
    glDeleteFramebuffers(1, &app->video_fbo);
    glDeleteTextures(1, &app->video_tex);
    glDeleteFramebuffers(1, &app->scene_fbo);
    glDeleteTextures(1, &app->scene_tex);
    app->video_fbo = app->video_tex = 0;
    app->scene_fbo = app->scene_tex = 0;
}

/* ------------------------------------------------------------------ */
/* Fullscreen quad                                                     */
/* ------------------------------------------------------------------ */
static void create_fullscreen_quad(GLuint *vao, GLuint *vbo) {
    float verts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);
}

static void draw_fullscreen_quad(GLuint vao) {
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

/* ------------------------------------------------------------------ */
/* File opening helpers                                                */
/* ------------------------------------------------------------------ */
static void try_auto_load_danmaku(App *app, const char *video_path) {
    /* Try to find danmaku file with same name but .xml/.json/.ass extension */
    char buf[1024];
    const char *exts[] = { ".xml", ".json", ".ass", NULL };
    const char *dot = strrchr(video_path, '.');

    size_t base_len;
    if (dot) {
        base_len = (size_t)(dot - video_path);
    } else {
        base_len = strlen(video_path);
    }

    for (int i = 0; exts[i]; i++) {
        if (base_len + strlen(exts[i]) >= sizeof(buf)) continue;
        memcpy(buf, video_path, base_len);
        strcpy(buf + base_len, exts[i]);

        FILE *fp = fopen(buf, "rb");
        if (fp) {
            fclose(fp);
            open_danmaku(app, buf);
            printf("[Main] Auto-loaded danmaku: %s\n", buf);
            return;
        }
    }
}

static void open_video(App *app, const char *path) {
    if (player_load(app->player, path)) {
        strncpy(app->config.last_file, path, sizeof(app->config.last_file) - 1);
        app->config.last_position = 0;

        /* Update window title */
        const char *title = player_get_title(app->player);
        char wt[256];
        snprintf(wt, sizeof(wt), "BiliBC - %s", title);
        SDL_SetWindowTitle(app->window, wt);

        /* Try auto-load danmaku */
        try_auto_load_danmaku(app, path);
    }
}

static void open_danmaku(App *app, const char *path) {
    danmaku_mgr_load(&app->danmaku, path);
    danmaku_mgr_reset(&app->danmaku, player_get_time(app->player));
}

/* ------------------------------------------------------------------ */
/* Time formatting                                                     */
/* ------------------------------------------------------------------ */
static void format_time(double seconds, char *buf, size_t buf_size) {
    if (seconds < 0) seconds = 0;
    int h = (int)(seconds / 3600);
    int m = (int)((seconds - h * 3600) / 60);
    int s = (int)(seconds - h * 3600 - m * 60);
    if (h > 0)
        snprintf(buf, buf_size, "%d:%02d:%02d", h, m, s);
    else
        snprintf(buf, buf_size, "%d:%02d", m, s);
}

/* ------------------------------------------------------------------ */
/* Screenshot with danmaku                                             */
/* ------------------------------------------------------------------ */
static void take_screenshot_with_danmaku(App *app) {
    char *path = platform_save_file_dialog("screenshot.png");
    if (!path) return;

    int w = app->width, h = app->height;
    unsigned char *pixels = (unsigned char *)malloc(w * h * 4);
    if (!pixels) { free(path); return; }

    /* Read from scene FBO (video + danmaku) */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, app->scene_fbo);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    /* Flip vertically */
    int stride = w * 4;
    unsigned char *row = (unsigned char *)malloc(stride);
    for (int y = 0; y < h / 2; y++) {
        memcpy(row, pixels + y * stride, stride);
        memcpy(pixels + y * stride, pixels + (h - 1 - y) * stride, stride);
        memcpy(pixels + (h - 1 - y) * stride, row, stride);
    }
    free(row);

    stbi_write_png(path, w, h, 4, pixels, stride);
    printf("[Screenshot] Saved: %s\n", path);

    free(pixels);
    free(path);
}

/* ------------------------------------------------------------------ */
/* Initialization                                                      */
/* ------------------------------------------------------------------ */
static bool app_init(App *app, int argc, char **argv) {
    memset(app, 0, sizeof(*app));
    app->running = true;

    /* Load config */
    config_load(&app->config);
    i18n_init(app->config.language);

    /* SDL init */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "[SDL] Init failed: %s\n", SDL_GetError());
        return false;
    }

    /* OpenGL attributes */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    /* Create window */
    int win_x = app->config.window_x >= 0 ? app->config.window_x : SDL_WINDOWPOS_CENTERED;
    int win_y = app->config.window_y >= 0 ? app->config.window_y : SDL_WINDOWPOS_CENTERED;
    app->width = app->config.window_w;
    app->height = app->config.window_h;

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (app->config.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    app->window = SDL_CreateWindow(
        _("app.title"),
        win_x, win_y,
        app->width, app->height,
        flags
    );
    if (!app->window) {
        fprintf(stderr, "[SDL] Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    /* Create GL context */
    app->gl_ctx = SDL_GL_CreateContext(app->window);
    if (!app->gl_ctx) {
        fprintf(stderr, "[SDL] GL context failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(app->window, app->gl_ctx);
    SDL_GL_SetSwapInterval(1); /* VSync */

    /* Load GL functions */
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "[GLAD] Failed to load GL\n");
        return false;
    }
    printf("[GL] %s / %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER));

    /* Enable file drag-and-drop */
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    /* Create player */
    app->player = player_create();
    if (!app->player) return false;

    if (!player_init_render(app->player, gl_get_proc, NULL)) {
        return false;
    }

    /* Apply config to player */
    player_set_volume(app->player, app->config.volume);
    player_set_speed(app->player, app->config.speed);

    /* Create FBOs */
    app_create_fbos(app);

    /* Load shaders */
    app->quad_prog = shader_load("shaders/quad.vert", "shaders/quad.frag");
    app->composite_prog = shader_load("shaders/quad.vert", "shaders/composite.frag");
    create_fullscreen_quad(&app->quad_vao, &app->quad_vbo);

    /* Create font & text renderer */
    app->font = font_ctx_create(NULL, 28);
    if (app->font) {
        app->text_renderer = text_renderer_create(app->font);
    }

    /* Create blur pipeline */
    app->blur = blur_create(app->width, app->height);

    /* Init UI */
    ui_init(&app->ui);

    /* Init danmaku manager */
    danmaku_mgr_init(&app->danmaku, app->width, app->height);
    app->danmaku.enabled = app->config.danmaku_enabled;
    app->danmaku.opacity = app->config.danmaku_opacity;
    app->danmaku.speed_factor = app->config.danmaku_speed;

    /* Timing */
    app->last_tick = SDL_GetPerformanceCounter();

    /* Handle command-line arguments: first arg is video file */
    for (int i = 1; i < argc; i++) {
        const char *ext = strrchr(argv[i], '.');
        if (ext && (strcasecmp(ext, ".xml") == 0 ||
                    strcasecmp(ext, ".json") == 0 ||
                    strcasecmp(ext, ".ass") == 0)) {
            open_danmaku(app, argv[i]);
        } else {
            open_video(app, argv[i]);
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* Shutdown                                                            */
/* ------------------------------------------------------------------ */
static void app_shutdown(App *app) {
    /* Save config (only if we have a valid window) */
    if (app->window) {
        SDL_GetWindowPosition(app->window, &app->config.window_x, &app->config.window_y);
        SDL_GetWindowSize(app->window, &app->config.window_w, &app->config.window_h);
        app->config.fullscreen = (SDL_GetWindowFlags(app->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    }
    if (app->player) {
        app->config.volume = player_get_volume(app->player);
        app->config.speed = player_get_speed(app->player);
        app->config.last_position = player_get_time(app->player);
    }
    app->config.danmaku_enabled = app->danmaku.enabled;
    app->config.danmaku_opacity = app->danmaku.opacity;
    app->config.danmaku_speed = app->danmaku.speed_factor;
    config_save(&app->config);

    danmaku_mgr_free(&app->danmaku);
    ui_destroy(&app->ui);
    blur_destroy(app->blur);
    text_renderer_destroy(app->text_renderer);
    font_ctx_destroy(app->font);
    app_destroy_fbos(app);

    if (app->quad_prog) glDeleteProgram(app->quad_prog);
    if (app->composite_prog) glDeleteProgram(app->composite_prog);
    if (app->quad_vao) glDeleteVertexArrays(1, &app->quad_vao);
    if (app->quad_vbo) glDeleteBuffers(1, &app->quad_vbo);

    player_destroy(app->player);
    if (app->gl_ctx) SDL_GL_DeleteContext(app->gl_ctx);
    if (app->window) SDL_DestroyWindow(app->window);
    SDL_Quit();
}

/* ------------------------------------------------------------------ */
/* Event handling                                                      */
/* ------------------------------------------------------------------ */
static void app_handle_event(App *app, SDL_Event *e) {
    switch (e->type) {
    case SDL_QUIT:
        app->running = false;
        break;

    case SDL_WINDOWEVENT:
        if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
            e->window.event == SDL_WINDOWEVENT_RESIZED) {
            app->width = e->window.data1;
            app->height = e->window.data2;
            app_destroy_fbos(app);
            app_create_fbos(app);
            if (app->blur) blur_resize(app->blur, app->width, app->height);
        }
        break;

    case SDL_DROPFILE: {
        char *file = e->drop.file;
        const char *ext = strrchr(file, '.');
        if (ext && (strcasecmp(ext, ".xml") == 0 ||
                    strcasecmp(ext, ".json") == 0 ||
                    strcasecmp(ext, ".ass") == 0)) {
            open_danmaku(app, file);
        } else {
            open_video(app, file);
        }
        SDL_free(file);
        break;
    }

    case SDL_MOUSEMOTION:
        app->mouse_moved = true;
        app->last_mouse_x = e->motion.x;
        app->last_mouse_y = e->motion.y;
        break;

    case SDL_KEYDOWN: {
        SDL_Keymod mod = SDL_GetModState();
        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool shift = (mod & KMOD_SHIFT) != 0;

        /* URL input mode */
        if (app->url_input_active) {
            if (e->key.keysym.sym == SDLK_ESCAPE) {
                app->url_input_active = false;
            } else if (e->key.keysym.sym == SDLK_RETURN) {
                if (app->url_len > 0) {
                    open_video(app, app->url_buf);
                }
                app->url_input_active = false;
            } else if (e->key.keysym.sym == SDLK_BACKSPACE && app->url_len > 0) {
                app->url_buf[--app->url_len] = 0;
            } else if (ctrl && e->key.keysym.sym == SDLK_v) {
                char *clip = platform_get_clipboard_text();
                if (clip) {
                    int clen = (int)strlen(clip);
                    int space = (int)sizeof(app->url_buf) - 1 - app->url_len;
                    if (clen > space) clen = space;
                    memcpy(app->url_buf + app->url_len, clip, clen);
                    app->url_len += clen;
                    app->url_buf[app->url_len] = 0;
                    free(clip);
                }
            }
            break;
        }

        switch (e->key.keysym.sym) {
        case SDLK_SPACE:
            player_toggle_pause(app->player);
            break;
        case SDLK_LEFT:
            player_seek(app->player, shift ? -30.0 : -5.0);
            danmaku_mgr_reset(&app->danmaku, player_get_time(app->player));
            break;
        case SDLK_RIGHT:
            player_seek(app->player, shift ? 30.0 : 5.0);
            danmaku_mgr_reset(&app->danmaku, player_get_time(app->player));
            break;
        case SDLK_UP:
            player_set_volume(app->player, player_get_volume(app->player) + 5);
            break;
        case SDLK_DOWN:
            player_set_volume(app->player, player_get_volume(app->player) - 5);
            break;
        case SDLK_f:
            app->fullscreen = !app->fullscreen;
            SDL_SetWindowFullscreen(app->window,
                app->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            break;
        case SDLK_o:
            if (ctrl) {
                /* Open danmaku dialog */
                char *path = platform_open_danmaku_dialog();
                if (path) { open_danmaku(app, path); free(path); }
            } else {
                /* Open video dialog */
                char *path = platform_open_file_dialog(NULL, NULL);
                if (path) { open_video(app, path); free(path); }
            }
            break;
        case SDLK_d:
            app->danmaku.enabled = !app->danmaku.enabled;
            break;
        case SDLK_s:
            if (ctrl && shift) {
                /* Screenshot without danmaku (mpv native) */
                player_screenshot(app->player, "screenshot_clean.png");
            } else if (ctrl) {
                /* Screenshot with danmaku */
                take_screenshot_with_danmaku(app);
            }
            break;
        case SDLK_u:
            /* Open URL input */
            app->url_input_active = true;
            app->url_len = 0;
            app->url_buf[0] = 0;
            break;
        case SDLK_l:
            /* Toggle language */
            if (i18n_get_language() == LANG_EN)
                i18n_set_language(LANG_ZH);
            else
                i18n_set_language(LANG_EN);
            break;
        case SDLK_ESCAPE:
            if (app->fullscreen) {
                app->fullscreen = false;
                SDL_SetWindowFullscreen(app->window, 0);
            }
            break;
        }
        break;
    }

    case SDL_TEXTINPUT:
        if (app->url_input_active) {
            int len = (int)strlen(e->text.text);
            int space = (int)sizeof(app->url_buf) - 1 - app->url_len;
            if (len > space) len = space;
            memcpy(app->url_buf + app->url_len, e->text.text, len);
            app->url_len += len;
            app->url_buf[app->url_len] = 0;
        }
        break;

    case SDL_MOUSEWHEEL:
        /* Volume with mouse wheel */
        player_set_volume(app->player, player_get_volume(app->player) + e->wheel.y * 5);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */
static void app_update(App *app, float dt) {
    /* Process mpv events */
    player_process_events(app->player);

    /* Detect seeks (large time jumps) */
    double cur_time = player_get_time(app->player);
    if (fabs(cur_time - app->prev_player_time) > 1.0 && app->prev_player_time > 0) {
        danmaku_mgr_reset(&app->danmaku, cur_time);
    }
    app->prev_player_time = cur_time;

    /* Update danmaku */
    danmaku_mgr_update(&app->danmaku, cur_time, dt, app->width, app->height);

    /* Update UI bar visibility */
    ui_update_bar(&app->ui, dt, app->mouse_moved);
    app->mouse_moved = false;
}

/* ------------------------------------------------------------------ */
/* Render                                                              */
/* ------------------------------------------------------------------ */
static void app_render(App *app) {
    int w = app->width, h = app->height;

    /* --- Pass 1: Render video to FBO --- */
    player_render(app->player, app->video_fbo, w, h);

    /* --- Pass 2: Render video + danmaku to scene FBO --- */
    glBindFramebuffer(GL_FRAMEBUFFER, app->scene_fbo);
    glViewport(0, 0, w, h);

    /* Draw video quad */
    glUseProgram(app->quad_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, app->video_tex);
    glUniform1i(glGetUniformLocation(app->quad_prog, "uTexture"), 0);
    glUniform1f(glGetUniformLocation(app->quad_prog, "uAlpha"), 1.0f);
    draw_fullscreen_quad(app->quad_vao);

    /* Draw danmaku on top */
    if (app->danmaku.enabled && app->text_renderer) {
        int count;
        const ActiveDanmaku *active = danmaku_mgr_get_active(&app->danmaku, &count);

        text_renderer_begin(app->text_renderer, w, h);

        for (int i = 0; i < count; i++) {
            if (!active[i].active) continue;
            const DanmakuItem *item = active[i].item;
            float scale = (float)item->fontsize / 28.0f *
                          (float)app->danmaku.font_scale / 100.0f;

            text_renderer_draw(app->text_renderer,
                               item->text,
                               active[i].x, active[i].y,
                               scale,
                               item->color,
                               app->danmaku.opacity);
        }

        text_renderer_end(app->text_renderer);
    }

    /* Draw idle hint if no video */
    if (player_is_idle(app->player) && app->text_renderer) {
        text_renderer_begin(app->text_renderer, w, h);
        const char *hint = _("app.drop_hint");
        float hw = font_measure_text(app->font, hint, 1.0f);
        text_renderer_draw(app->text_renderer, hint,
                           (w - hw) * 0.5f, h * 0.5f, 1.0f,
                           0xAAAAAA, 0.8f);
        text_renderer_end(app->text_renderer);
    }

    /* Draw URL input box if active */
    if (app->url_input_active && app->text_renderer) {
        float box_w = 600, box_h = 40;
        float box_x = (w - box_w) * 0.5f;
        float box_y = h * 0.4f;

        ui_draw_rect(&app->ui, box_x, box_y, box_w, box_h, 8,
                     0.1f, 0.1f, 0.1f, 0.85f);

        text_renderer_begin(app->text_renderer, w, h);
        if (app->url_len > 0) {
            text_renderer_draw(app->text_renderer, app->url_buf,
                               box_x + 10, box_y + 8, 0.7f, 0xFFFFFF, 1.0f);
        } else {
            text_renderer_draw(app->text_renderer, _("msg.url_hint"),
                               box_x + 10, box_y + 8, 0.7f, 0x888888, 0.6f);
        }
        text_renderer_end(app->text_renderer);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    /* --- Pass 3: Blur for control bar frosted glass --- */
    GLuint blurred_tex = 0;
    if (app->blur && app->ui.bar_alpha > 0.01f) {
        blurred_tex = blur_apply(app->blur, app->scene_tex, w, h);
    }

    /* --- Pass 4: Composite to screen --- */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    if (app->composite_prog && blurred_tex) {
        glUseProgram(app->composite_prog);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app->scene_tex);
        glUniform1i(glGetUniformLocation(app->composite_prog, "uVideo"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blurred_tex);
        glUniform1i(glGetUniformLocation(app->composite_prog, "uBlurred"), 1);

        /* UI layer is not a separate texture in this impl; we draw it after */
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, app->scene_tex); /* placeholder */
        glUniform1i(glGetUniformLocation(app->composite_prog, "uUI"), 2);

        float bar_y = 80.0f / (float)h;
        glUniform1f(glGetUniformLocation(app->composite_prog, "uBarY"), bar_y);
        glUniform1f(glGetUniformLocation(app->composite_prog, "uBarAlpha"), app->ui.bar_alpha);

        draw_fullscreen_quad(app->quad_vao);
    } else {
        /* Fallback: just draw scene directly */
        glUseProgram(app->quad_prog);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, app->scene_tex);
        glUniform1i(glGetUniformLocation(app->quad_prog, "uTexture"), 0);
        glUniform1f(glGetUniformLocation(app->quad_prog, "uAlpha"), 1.0f);
        draw_fullscreen_quad(app->quad_vao);
    }

    /* --- Pass 5: Draw UI controls over everything --- */
    if (app->ui.bar_alpha > 0.01f) {
        int mx, my;
        Uint32 mb = SDL_GetMouseState(&mx, &my);
        ui_begin(&app->ui, mx, my, (mb & SDL_BUTTON_LMASK) != 0, w, h, 0);

        float bar_x = 0;
        float bar_y = (float)h - 80.0f;
        float bar_w = (float)w;
        float bar_h = 80.0f;
        float pad = 15.0f;

        /* Progress bar */
        double dur = player_get_duration(app->player);
        double cur = player_get_time(app->player);
        float progress = dur > 0 ? (float)(cur / dur) : 0;

        float seek = ui_progress_bar(&app->ui, 100,
                                      bar_x + pad, bar_y + 5,
                                      bar_w - pad * 2, 6,
                                      progress, 0);
        if (seek >= 0 && dur > 0) {
            double target = seek * dur;
            player_seek(app->player, target - cur);
            danmaku_mgr_reset(&app->danmaku, target);
        }

        float btn_y = bar_y + 30;
        float btn_size = 36;
        float cx = pad;

        /* Play/Pause button */
        bool paused = player_is_paused(app->player);
        if (ui_button(&app->ui, 1, cx, btn_y, btn_size, btn_size,
                      paused ? "Play" : "Pause", false)) {
            player_toggle_pause(app->player);
        }
        /* Draw play/pause icon with text */
        if (app->text_renderer && app->ui.bar_alpha > 0.01f) {
            text_renderer_begin(app->text_renderer, w, h);
            text_renderer_draw(app->text_renderer,
                               paused ? "\xe2\x96\xb6" : "\xe2\x8f\xb8",
                               cx + 8, btn_y + 6, 0.7f, 0xFFFFFF,
                               app->ui.bar_alpha);
            text_renderer_end(app->text_renderer);
        }
        cx += btn_size + 10;

        /* Time display */
        if (app->text_renderer && app->ui.bar_alpha > 0.01f) {
            char time_str[64];
            char dur_str[32], cur_str[32];
            format_time(cur, cur_str, sizeof(cur_str));
            format_time(dur, dur_str, sizeof(dur_str));
            snprintf(time_str, sizeof(time_str), "%s / %s", cur_str, dur_str);

            text_renderer_begin(app->text_renderer, w, h);
            text_renderer_draw(app->text_renderer, time_str,
                               cx, btn_y + 8, 0.55f, 0xCCCCCC,
                               app->ui.bar_alpha);
            text_renderer_end(app->text_renderer);
            cx += 140;
        }

        /* Right side controls */
        float rx = bar_w - pad;

        /* Fullscreen button */
        rx -= btn_size;
        if (ui_button(&app->ui, 2, rx, btn_y, btn_size, btn_size, "FS", false)) {
            app->fullscreen = !app->fullscreen;
            SDL_SetWindowFullscreen(app->window,
                app->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        }

        /* Volume slider */
        rx -= 110;
        int vol = player_get_volume(app->player);
        float new_vol = ui_slider(&app->ui, 3, rx, btn_y + 4, 90, btn_size - 8,
                                   (float)vol / 100.0f);
        if ((int)(new_vol * 100) != vol) {
            player_set_volume(app->player, (int)(new_vol * 100));
        }
        rx -= 10;

        /* Danmaku toggle */
        rx -= 50;
        bool dm_on = ui_toggle(&app->ui, 4, rx, btn_y + 8, app->danmaku.enabled);
        app->danmaku.enabled = dm_on;

        /* Danmaku label */
        if (app->text_renderer && app->ui.bar_alpha > 0.01f) {
            text_renderer_begin(app->text_renderer, w, h);
            text_renderer_draw(app->text_renderer, _("ctrl.danmaku"),
                               rx - 45, btn_y + 10, 0.5f, 0xCCCCCC,
                               app->ui.bar_alpha);
            text_renderer_end(app->text_renderer);
        }

        /* Speed button */
        rx -= 90;
        double spd = player_get_speed(app->player);
        char spd_str[16];
        snprintf(spd_str, sizeof(spd_str), "%.1fx", spd);
        if (ui_button(&app->ui, 5, rx, btn_y, 40, btn_size, spd_str, false)) {
            app->speed_menu_open = !app->speed_menu_open;
        }
        if (app->text_renderer && app->ui.bar_alpha > 0.01f) {
            text_renderer_begin(app->text_renderer, w, h);
            text_renderer_draw(app->text_renderer, spd_str,
                               rx + 4, btn_y + 8, 0.5f, 0xFFFFFF,
                               app->ui.bar_alpha);
            text_renderer_end(app->text_renderer);
        }

        /* Speed menu popup */
        if (app->speed_menu_open && app->ui.bar_alpha > 0.5f) {
            static const double speeds[] = { 0.25, 0.5, 1.0, 1.5, 2.0 };
            float menu_x = rx - 10;
            float menu_y = btn_y - 5 * 30 - 10;
            ui_draw_rect(&app->ui, menu_x, menu_y, 60, 5 * 30 + 10, 8,
                         0.1f, 0.1f, 0.1f, 0.85f * app->ui.bar_alpha);

            for (int i = 0; i < 5; i++) {
                char label[16];
                snprintf(label, sizeof(label), "%.2gx", speeds[i]);
                bool is_active = (fabs(spd - speeds[i]) < 0.01);
                if (ui_button(&app->ui, 10 + i,
                              menu_x + 5, menu_y + 5 + i * 30,
                              50, 26, label, is_active)) {
                    player_set_speed(app->player, speeds[i]);
                    app->speed_menu_open = false;
                }
                if (app->text_renderer) {
                    text_renderer_begin(app->text_renderer, w, h);
                    text_renderer_draw(app->text_renderer, label,
                                       menu_x + 12, menu_y + 10 + i * 30,
                                       0.5f,
                                       is_active ? 0x00A6FF : 0xCCCCCC,
                                       app->ui.bar_alpha);
                    text_renderer_end(app->text_renderer);
                }
            }
        }

        ui_end(&app->ui);
    }

    /* Swap */
    SDL_GL_SwapWindow(app->window);
    player_report_flip(app->player);
}

/* ------------------------------------------------------------------ */
/* Main entry point                                                    */
/* ------------------------------------------------------------------ */
int main(int argc, char **argv) {
    App app;

    if (!app_init(&app, argc, argv)) {
        fprintf(stderr, "Failed to initialize BiliBC\n");
        app_shutdown(&app);
        return 1;
    }

    while (app.running) {
        /* Calculate delta time */
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - app.last_tick) / (float)SDL_GetPerformanceFrequency();
        app.last_tick = now;
        if (dt > 0.1f) dt = 0.1f;  /* clamp */

        /* Handle events */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            app_handle_event(&app, &e);
        }

        /* Update & render */
        app_update(&app, dt);
        app_render(&app);
    }

    app_shutdown(&app);
    return 0;
}
