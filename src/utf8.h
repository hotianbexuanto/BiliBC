/* utf8.h - UTF-8 decode iterator */
#ifndef UTF8_H
#define UTF8_H

#include <stdint.h>
#include <stddef.h>

/* Decode the next UTF-8 codepoint from *str.
 * Advances *str past the decoded bytes.
 * Returns the codepoint, or 0xFFFD on error, or 0 at end. */
uint32_t utf8_decode(const char **str);

/* Count the number of codepoints in a UTF-8 string. */
size_t utf8_strlen(const char *str);

#endif /* UTF8_H */
