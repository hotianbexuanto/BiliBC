/* yxml - A small, fast, and correct XML parser
 * https://dev.yorhel.nl/yxml
 *
 * Copyright (c) 2013-2014 Yoran Heling
 * SPDX-License-Identifier: MIT
 */
#ifndef YXML_H
#define YXML_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    YXML_EEOF        = -5,
    YXML_EREF        = -4,
    YXML_ECLOSE      = -3,
    YXML_ESTACK      = -2,
    YXML_ESYN        = -1,
    YXML_OK          =  0,
    YXML_ELEMSTART   =  1,
    YXML_CONTENT     =  2,
    YXML_ELEMEND     =  3,
    YXML_ATTRSTART   =  4,
    YXML_ATTRVAL     =  5,
    YXML_ATTREND     =  6,
    YXML_PISTART     =  7,
    YXML_PICONTENT   =  8,
    YXML_PIEND       =  9
} yxml_ret_t;

typedef struct {
    char *elem;
    char *attr;
    char data[8];
    char pi[8];
    uint64_t byte;
    uint64_t total;
    uint32_t line;
    int state;
    char *stack;
    size_t stacksize;
    size_t stacklen;
    int refstate;
    unsigned int ref;
    char quote;
    int nextstate;
    unsigned int ignore;
    unsigned char *string;
} yxml_t;

void yxml_init(yxml_t *x, void *stack, size_t stacksize);
yxml_ret_t yxml_parse(yxml_t *x, int ch);
yxml_ret_t yxml_eof(yxml_t *x);

static inline const char *yxml_symlen(yxml_t *x, unsigned *len) {
    (void)x; (void)len;
    return NULL;
}

#endif
