/* player.h - libmpv video playback wrapper */
#ifndef PLAYER_H
#define PLAYER_H

#include <glad/glad.h>
#include <stdbool.h>

typedef struct Player Player;

/* Create player. Returns NULL on failure. */
Player *player_create(void);

/* Destroy player and free resources. */
void player_destroy(Player *p);

/* Initialize the mpv OpenGL render context. Call after GL context is ready. */
bool player_init_render(Player *p, void *(*get_proc)(void *ctx, const char *name), void *ctx);

/* Load and play a file or URL. */
bool player_load(Player *p, const char *path);

/* Render current frame into the given FBO at the given size. */
void player_render(Player *p, GLuint fbo, int width, int height);

/* Report that the host has flipped (swapped) buffers. */
void player_report_flip(Player *p);

/* Playback controls */
void player_toggle_pause(Player *p);
void player_seek(Player *p, double seconds);     /* relative seek */
void player_set_volume(Player *p, int volume);    /* 0..150 */
int  player_get_volume(Player *p);
void player_set_speed(Player *p, double speed);   /* 0.25..4.0 */
double player_get_speed(Player *p);

/* State queries */
double player_get_time(Player *p);          /* current time in seconds */
double player_get_duration(Player *p);      /* total duration */
bool   player_is_paused(Player *p);
bool   player_is_idle(Player *p);           /* no file loaded */
const char *player_get_title(Player *p);

/* Process mpv events. Returns true if redraw is needed. */
bool player_process_events(Player *p);

/* Screenshot (mpv's own, without danmaku) */
void player_screenshot(Player *p, const char *filename);

#endif /* PLAYER_H */
