/* config.c - Configuration persistence (JSON) */
#include "config.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform function (from platform_win32.c) */
extern char *platform_get_config_path(void);

void config_defaults(AppConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->window_x = -1;  /* centered */
    cfg->window_y = -1;
    cfg->window_w = 1280;
    cfg->window_h = 720;
    cfg->fullscreen = false;
    cfg->volume = 100;
    cfg->speed = 1.0;
    cfg->danmaku_enabled = true;
    cfg->danmaku_opacity = 1.0f;
    cfg->danmaku_speed = 1.0f;
    cfg->danmaku_font_scale = 100;
    cfg->language = LANG_ZH;  /* Default to Chinese */
}

void config_load(AppConfig *cfg) {
    config_defaults(cfg);

    char *path = platform_get_config_path();
    if (!path) return;

    FILE *fp = fopen(path, "rb");
    free(path);
    if (!fp) return;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *data = (char *)malloc(sz + 1);
    fread(data, 1, sz, fp);
    data[sz] = 0;
    fclose(fp);

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) return;

    cJSON *j;
    if ((j = cJSON_GetObjectItem(root, "window_x")) && cJSON_IsNumber(j)) cfg->window_x = j->valueint;
    if ((j = cJSON_GetObjectItem(root, "window_y")) && cJSON_IsNumber(j)) cfg->window_y = j->valueint;
    if ((j = cJSON_GetObjectItem(root, "window_w")) && cJSON_IsNumber(j)) cfg->window_w = j->valueint;
    if ((j = cJSON_GetObjectItem(root, "window_h")) && cJSON_IsNumber(j)) cfg->window_h = j->valueint;
    if ((j = cJSON_GetObjectItem(root, "fullscreen")) && cJSON_IsBool(j)) cfg->fullscreen = cJSON_IsTrue(j);
    if ((j = cJSON_GetObjectItem(root, "volume")) && cJSON_IsNumber(j)) cfg->volume = j->valueint;
    if ((j = cJSON_GetObjectItem(root, "speed")) && cJSON_IsNumber(j)) cfg->speed = j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "last_file")) && cJSON_IsString(j)) {
        strncpy(cfg->last_file, j->valuestring, sizeof(cfg->last_file) - 1);
    }
    if ((j = cJSON_GetObjectItem(root, "last_position")) && cJSON_IsNumber(j)) cfg->last_position = j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "danmaku_enabled")) && cJSON_IsBool(j)) cfg->danmaku_enabled = cJSON_IsTrue(j);
    if ((j = cJSON_GetObjectItem(root, "danmaku_opacity")) && cJSON_IsNumber(j)) cfg->danmaku_opacity = (float)j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "danmaku_speed")) && cJSON_IsNumber(j)) cfg->danmaku_speed = (float)j->valuedouble;
    if ((j = cJSON_GetObjectItem(root, "danmaku_font_scale")) && cJSON_IsNumber(j)) cfg->danmaku_font_scale = j->valueint;
    if ((j = cJSON_GetObjectItem(root, "language")) && cJSON_IsNumber(j)) cfg->language = (Language)j->valueint;

    cJSON_Delete(root);
    printf("[Config] Loaded settings\n");
}

void config_save(const AppConfig *cfg) {
    char *path = platform_get_config_path();
    if (!path) return;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "window_x", cfg->window_x);
    cJSON_AddNumberToObject(root, "window_y", cfg->window_y);
    cJSON_AddNumberToObject(root, "window_w", cfg->window_w);
    cJSON_AddNumberToObject(root, "window_h", cfg->window_h);
    cJSON_AddBoolToObject(root, "fullscreen", cfg->fullscreen);
    cJSON_AddNumberToObject(root, "volume", cfg->volume);
    cJSON_AddNumberToObject(root, "speed", cfg->speed);
    cJSON_AddStringToObject(root, "last_file", cfg->last_file);
    cJSON_AddNumberToObject(root, "last_position", cfg->last_position);
    cJSON_AddBoolToObject(root, "danmaku_enabled", cfg->danmaku_enabled);
    cJSON_AddNumberToObject(root, "danmaku_opacity", cfg->danmaku_opacity);
    cJSON_AddNumberToObject(root, "danmaku_speed", cfg->danmaku_speed);
    cJSON_AddNumberToObject(root, "danmaku_font_scale", cfg->danmaku_font_scale);
    cJSON_AddNumberToObject(root, "language", cfg->language);

    char *json = cJSON_Print(root);
    cJSON_Delete(root);

    if (json) {
        FILE *fp = fopen(path, "wb");
        if (fp) {
            fputs(json, fp);
            fclose(fp);
        }
        cJSON_free(json);
    }
    free(path);
}
