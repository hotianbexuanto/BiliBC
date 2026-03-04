/* platform_win32.c - Win32 platform utilities */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

/* Open file dialog. Returns allocated UTF-8 path or NULL. Caller must free(). */
char *platform_open_file_dialog(const char *title, const char *filter) {
    (void)title; (void)filter;

    OPENFILENAMEW ofn;
    wchar_t wpath[MAX_PATH] = { 0 };

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = wpath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.avi;*.webm;*.flv;*.mov;*.wmv;*.ts\0"
                      L"All Files\0*.*\0";
    ofn.lpstrTitle = L"Open Video File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return NULL;

    /* Convert wchar to UTF-8 */
    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *utf8 = (char *)malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, len, NULL, NULL);
    return utf8;
}

/* Open danmaku file dialog */
char *platform_open_danmaku_dialog(void) {
    OPENFILENAMEW ofn;
    wchar_t wpath[MAX_PATH] = { 0 };

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = wpath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Danmaku Files\0*.xml;*.json;*.ass\0"
                      L"XML Danmaku\0*.xml\0"
                      L"JSON Danmaku\0*.json\0"
                      L"ASS Subtitle\0*.ass\0"
                      L"All Files\0*.*\0";
    ofn.lpstrTitle = L"Open Danmaku File";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return NULL;

    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *utf8 = (char *)malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, len, NULL, NULL);
    return utf8;
}

/* Save file dialog for screenshots */
char *platform_save_file_dialog(const char *default_name) {
    OPENFILENAMEW ofn;
    wchar_t wpath[MAX_PATH] = { 0 };

    /* Convert default name to wide */
    if (default_name) {
        MultiByteToWideChar(CP_UTF8, 0, default_name, -1, wpath, MAX_PATH);
    }

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = wpath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"PNG Image\0*.png\0All Files\0*.*\0";
    ofn.lpstrTitle = L"Save Screenshot";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"png";

    if (!GetSaveFileNameW(&ofn)) return NULL;

    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *utf8 = (char *)malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, len, NULL, NULL);
    return utf8;
}

/* Get clipboard text as UTF-8. Caller must free(). */
char *platform_get_clipboard_text(void) {
    if (!OpenClipboard(NULL)) return NULL;

    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (!h) { CloseClipboard(); return NULL; }

    wchar_t *wtext = (wchar_t *)GlobalLock(h);
    if (!wtext) { CloseClipboard(); return NULL; }

    int len = WideCharToMultiByte(CP_UTF8, 0, wtext, -1, NULL, 0, NULL, NULL);
    char *utf8 = NULL;
    if (len > 0) {
        utf8 = (char *)malloc(len);
        WideCharToMultiByte(CP_UTF8, 0, wtext, -1, utf8, len, NULL, NULL);
    }

    GlobalUnlock(h);
    CloseClipboard();
    return utf8;
}

/* Get the path to save config (next to exe) */
char *platform_get_config_path(void) {
    wchar_t wpath[MAX_PATH];
    GetModuleFileNameW(NULL, wpath, MAX_PATH);

    /* Replace filename with config name */
    wchar_t *slash = wcsrchr(wpath, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat(wpath, L"bilibc_config.json");

    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *utf8 = (char *)malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, len, NULL, NULL);
    return utf8;
}

#else
/* Stub implementations for non-Windows */
#include <stdlib.h>
char *platform_open_file_dialog(const char *t, const char *f) { (void)t; (void)f; return NULL; }
char *platform_open_danmaku_dialog(void) { return NULL; }
char *platform_save_file_dialog(const char *d) { (void)d; return NULL; }
char *platform_get_clipboard_text(void) { return NULL; }
char *platform_get_config_path(void) { return NULL; }
#endif
