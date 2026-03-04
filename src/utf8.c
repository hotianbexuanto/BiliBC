/* utf8.c - UTF-8 decode iterator */
#include "utf8.h"

uint32_t utf8_decode(const char **str) {
    const unsigned char *s = (const unsigned char *)*str;
    if (*s == 0) return 0;

    uint32_t cp;
    int bytes;

    if (s[0] < 0x80) {
        cp = s[0]; bytes = 1;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = s[0] & 0x1F; bytes = 2;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = s[0] & 0x0F; bytes = 3;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = s[0] & 0x07; bytes = 4;
    } else {
        *str = (const char *)(s + 1);
        return 0xFFFD;
    }

    for (int i = 1; i < bytes; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *str = (const char *)(s + i);
            return 0xFFFD;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }

    *str = (const char *)(s + bytes);
    return cp;
}

size_t utf8_strlen(const char *str) {
    size_t count = 0;
    while (*str) {
        utf8_decode(&str);
        count++;
    }
    return count;
}
