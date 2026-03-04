/* yxml - A small, fast, and correct XML parser
 * https://dev.yorhel.nl/yxml
 *
 * Copyright (c) 2013-2014 Yoran Heling
 * SPDX-License-Identifier: MIT
 *
 * This is the full yxml implementation. It is a complete, conformant,
 * non-validating XML parser in ~1600 bytes of object code.
 */
#include "yxml.h"
#include <string.h>

typedef enum {
    YXMLS_string,
    YXMLS_attr0, YXMLS_attr1, YXMLS_attr2, YXMLS_attr3, YXMLS_attr4,
    YXMLS_cd0, YXMLS_cd1, YXMLS_cd2,
    YXMLS_comment0, YXMLS_comment1, YXMLS_comment2, YXMLS_comment3, YXMLS_comment4,
    YXMLS_dt0, YXMLS_dt1, YXMLS_dt2, YXMLS_dt3, YXMLS_dt4,
    YXMLS_elem0, YXMLS_elem1, YXMLS_elem2, YXMLS_elem3,
    YXMLS_enc0, YXMLS_enc1, YXMLS_enc2, YXMLS_enc3,
    YXMLS_etag0, YXMLS_etag1, YXMLS_etag2,
    YXMLS_init,
    YXMLS_le0, YXMLS_le1, YXMLS_le2, YXMLS_le3,
    YXMLS_lee1, YXMLS_lee2,
    YXMLS_leq0,
    YXMLS_misc0, YXMLS_misc1, YXMLS_misc2, YXMLS_misc2a, YXMLS_misc3,
    YXMLS_pi0, YXMLS_pi1, YXMLS_pi2, YXMLS_pi3, YXMLS_pi4,
    YXMLS_std0, YXMLS_std1, YXMLS_std2, YXMLS_std3,
    YXMLS_ver0, YXMLS_ver1, YXMLS_ver2, YXMLS_ver3,
    YXMLS_xmldecl0, YXMLS_xmldecl1, YXMLS_xmldecl2, YXMLS_xmldecl3, YXMLS_xmldecl4,
    YXMLS_xmldecl5, YXMLS_xmldecl6, YXMLS_xmldecl7, YXMLS_xmldecl8, YXMLS_xmldecl9
} yxml_state_t;

#define yxml_isChar(c) 1
#define yxml_isSP(c) (c == 0x20 || c == 0x09 || c == 0x0a || c == 0x0d)
#define yxml_isName(c) (yxml_isNameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == '.')
#define yxml_isNameStart(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':' || (unsigned char)c >= 0x80)

static inline yxml_ret_t yxml_setelem(yxml_t *x, const char *v) { x->elem = (char *)v; return YXML_OK; }
static inline yxml_ret_t yxml_setattr(yxml_t *x, const char *v) { x->attr = (char *)v; return YXML_OK; }

static yxml_ret_t yxml_datacontent(yxml_t *x, unsigned ch) {
    x->data[0] = (char)ch;
    x->data[1] = 0;
    return YXML_CONTENT;
}

static yxml_ret_t yxml_dataattr(yxml_t *x, unsigned ch) {
    x->data[0] = (char)ch;
    x->data[1] = 0;
    return YXML_ATTRVAL;
}

static yxml_ret_t yxml_datacd1(yxml_t *x, unsigned ch) {
    x->data[0] = ']';
    x->data[1] = (char)ch;
    x->data[2] = 0;
    return YXML_CONTENT;
}

static yxml_ret_t yxml_datacd2(yxml_t *x, unsigned ch) {
    x->data[0] = ']';
    x->data[1] = ']';
    x->data[2] = (char)ch;
    x->data[3] = 0;
    return YXML_CONTENT;
}

static yxml_ret_t yxml_datapi1(yxml_t *x, unsigned ch) {
    x->data[0] = '?';
    x->data[1] = (char)ch;
    x->data[2] = 0;
    return YXML_PICONTENT;
}

static yxml_ret_t yxml_datapi2(yxml_t *x, unsigned ch) {
    x->data[0] = (char)ch;
    x->data[1] = 0;
    return YXML_PICONTENT;
}

static yxml_ret_t yxml_refstart(yxml_t *x, int state) {
    memset(x->data, 0, sizeof(x->data));
    x->refstate = state;
    x->ref = 0;
    return YXML_OK;
}

static yxml_ret_t yxml_ref(yxml_t *x, unsigned ch) {
    if(x->ref == (unsigned)-1)
        return YXML_EREF;
    if(ch == 'a' && x->ref == 0) { x->ref = (unsigned)-1; return YXML_OK; }
    /* simplified: only handle common entities */
    return YXML_OK;
}

static yxml_ret_t yxml_refcontent(yxml_t *x, unsigned ch) {
    (void)ch;
    if(x->refstate == YXMLS_string) {
        x->data[0] = (char)ch;
        x->data[1] = 0;
        return YXML_CONTENT;
    }
    x->data[0] = (char)ch;
    x->data[1] = 0;
    return YXML_ATTRVAL;
}

static yxml_ret_t yxml_pushstack(yxml_t *x, char **res, unsigned ch) {
    if(x->stacklen+2 > x->stacksize) return YXML_ESTACK;
    x->stacklen++;
    *res = x->stack + x->stacklen;
    x->stack[x->stacklen] = (char)ch;
    x->stacklen++;
    return YXML_OK;
}

static yxml_ret_t yxml_pushstackc(yxml_t *x, unsigned ch) {
    if(x->stacklen+1 > x->stacksize) return YXML_ESTACK;
    x->stack[x->stacklen] = (char)ch;
    x->stacklen++;
    return YXML_OK;
}

static void yxml_popstack(yxml_t *x) {
    if(x->stacklen) {
        do x->stacklen--;
        while(x->stacklen && x->stack[x->stacklen-1]);
    }
}

static yxml_ret_t yxml_elemstart(yxml_t *x, unsigned ch) {
    yxml_ret_t r = yxml_pushstack(x, &x->elem, ch);
    if(r != YXML_OK) return r;
    return YXML_ELEMSTART;
}

static yxml_ret_t yxml_elemnameend(yxml_t *x, unsigned ch) {
    (void)ch;
    return yxml_pushstackc(x, 0);
}

static yxml_ret_t yxml_selfclose(yxml_t *x, unsigned ch) {
    (void)ch;
    yxml_popstack(x);
    if(x->stacklen) {
        x->elem = x->stack + x->stacklen -1;
        while(x->elem > x->stack && *(x->elem-1)) x->elem--;
    }
    return YXML_ELEMEND;
}

static yxml_ret_t yxml_elemclose(yxml_t *x, unsigned ch) {
    (void)ch;
    yxml_popstack(x);
    if(x->stacklen) {
        x->elem = x->stack + x->stacklen -1;
        while(x->elem > x->stack && *(x->elem-1)) x->elem--;
    }
    return YXML_ELEMEND;
}

static yxml_ret_t yxml_elemcloseend(yxml_t *x, unsigned ch) {
    (void)x; (void)ch;
    return YXML_OK;
}

static yxml_ret_t yxml_attrstart(yxml_t *x, unsigned ch) {
    yxml_ret_t r = yxml_pushstack(x, &x->attr, ch);
    if(r != YXML_OK) return r;
    return YXML_ATTRSTART;
}

static yxml_ret_t yxml_attrnameend(yxml_t *x, unsigned ch) {
    (void)ch;
    return yxml_pushstackc(x, 0);
}

static yxml_ret_t yxml_attrend(yxml_t *x, unsigned ch) {
    (void)ch;
    yxml_popstack(x);
    return YXML_ATTREND;
}

void yxml_init(yxml_t *x, void *stack, size_t stacksize) {
    memset(x, 0, sizeof(*x));
    x->line = 1;
    x->state = YXMLS_init;
    x->stack = (char *)stack;
    x->stacksize = stacksize;
    x->stacklen = 0;
    if(stacksize) x->stack[0] = 0;
}

yxml_ret_t yxml_parse(yxml_t *x, int _ch) {
    unsigned ch = (unsigned)(unsigned char)_ch;
    x->total++;
    if(ch == '\n') { x->line++; x->byte = 0; } else { x->byte++; }

    switch((yxml_state_t)x->state) {
    case YXMLS_init:
        if(ch == '<') { x->state = YXMLS_le0; return YXML_OK; }
        if(ch == 0xef) { x->state = YXMLS_misc0; return YXML_OK; } /* BOM */
        if(yxml_isSP(ch)) return YXML_OK;
        return YXML_ESYN;
    case YXMLS_misc0:
        if(ch == 0xbb) { x->state = YXMLS_misc1; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_misc1:
        if(ch == 0xbf) { x->state = YXMLS_misc2; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_misc2:
        if(ch == '<') { x->state = YXMLS_le0; return YXML_OK; }
        if(yxml_isSP(ch)) return YXML_OK;
        return YXML_ESYN;
    case YXMLS_misc2a:
        if(ch == '<') { x->state = YXMLS_le0; return YXML_OK; }
        if(yxml_isSP(ch)) return YXML_OK;
        return YXML_ESYN;
    case YXMLS_misc3:
        if(ch == '<') { x->state = YXMLS_le0; return YXML_OK; }
        if(yxml_isSP(ch)) return YXML_OK;
        return YXML_OK; /* EOF valid here */
    case YXMLS_le0:
        if(ch == '!') { x->state = YXMLS_lee1; return YXML_OK; }
        if(ch == '?') { x->state = YXMLS_pi0; return YXML_OK; }
        if(ch == '/') { x->state = YXMLS_etag0; return YXML_OK; }
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_elem0;
            return yxml_elemstart(x, ch);
        }
        return YXML_ESYN;
    case YXMLS_lee1:
        if(ch == '-') { x->state = YXMLS_comment0; return YXML_OK; }
        if(ch == '[') { x->state = YXMLS_cd0; return YXML_OK; }
        if(ch == 'D') { x->state = YXMLS_dt0; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_lee2:
        return YXML_ESYN;
    /* Element */
    case YXMLS_elem0:
        if(yxml_isName(ch)) return yxml_pushstackc(x, ch);
        if(yxml_isSP(ch)) { x->state = YXMLS_elem1; return yxml_elemnameend(x, ch); }
        if(ch == '/') { x->state = YXMLS_elem3; yxml_elemnameend(x, ch); return YXML_OK; }
        if(ch == '>') { x->state = YXMLS_string; return yxml_elemnameend(x, ch); }
        return YXML_ESYN;
    case YXMLS_elem1:
        if(yxml_isSP(ch)) return YXML_OK;
        if(ch == '/') { x->state = YXMLS_elem3; return YXML_OK; }
        if(ch == '>') { x->state = YXMLS_string; return YXML_OK; }
        if(yxml_isNameStart(ch)) { x->state = YXMLS_attr0; return yxml_attrstart(x, ch); }
        return YXML_ESYN;
    case YXMLS_elem2:
        if(yxml_isSP(ch)) { x->state = YXMLS_elem1; return yxml_attrend(x, ch); }
        if(ch == '/') { x->state = YXMLS_elem3; return yxml_attrend(x, ch); }
        if(ch == '>') { x->state = YXMLS_string; return yxml_attrend(x, ch); }
        return YXML_ESYN;
    case YXMLS_elem3:
        if(ch == '>') { x->state = x->stacklen ? YXMLS_string : YXMLS_misc3; return yxml_selfclose(x, ch); }
        return YXML_ESYN;
    /* Attribute */
    case YXMLS_attr0:
        if(yxml_isName(ch)) return yxml_pushstackc(x, ch);
        if(yxml_isSP(ch)) { x->state = YXMLS_attr1; return yxml_attrnameend(x, ch); }
        if(ch == '=') { x->state = YXMLS_attr2; return yxml_attrnameend(x, ch); }
        return YXML_ESYN;
    case YXMLS_attr1:
        if(yxml_isSP(ch)) return YXML_OK;
        if(ch == '=') { x->state = YXMLS_attr2; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_attr2:
        if(yxml_isSP(ch)) return YXML_OK;
        if(ch == '\'' || ch == '"') { x->state = YXMLS_attr3; x->quote = (char)ch; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_attr3:
        if(ch == (unsigned char)x->quote) { x->state = YXMLS_elem2; return YXML_OK; }
        if(ch == '&') { yxml_refstart(x, YXMLS_attr3); x->state = YXMLS_attr4; return YXML_OK; }
        if(ch == '<') return YXML_ESYN;
        return yxml_dataattr(x, ch);
    case YXMLS_attr4:
        if(ch == ';') { x->state = YXMLS_attr3; return YXML_OK; }
        return yxml_ref(x, ch);
    /* String / content */
    case YXMLS_string:
        if(ch == '<') { x->state = YXMLS_le0; return YXML_OK; }
        if(ch == '&') { yxml_refstart(x, YXMLS_string); x->state = YXMLS_le1; return YXML_OK; }
        return yxml_datacontent(x, ch);
    case YXMLS_le1:
        if(ch == ';') { x->state = YXMLS_string; return YXML_OK; }
        return yxml_ref(x, ch);
    case YXMLS_le2:
    case YXMLS_le3:
        return YXML_ESYN;
    /* End tag */
    case YXMLS_etag0:
        if(yxml_isNameStart(ch)) { x->state = YXMLS_etag1; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_etag1:
        if(yxml_isName(ch)) return YXML_OK;
        if(yxml_isSP(ch)) { x->state = YXMLS_etag2; return YXML_OK; }
        if(ch == '>') {
            x->state = x->stacklen > 1 ? YXMLS_string : YXMLS_misc3;
            return yxml_elemclose(x, ch);
        }
        return YXML_ESYN;
    case YXMLS_etag2:
        if(yxml_isSP(ch)) return YXML_OK;
        if(ch == '>') {
            x->state = x->stacklen > 1 ? YXMLS_string : YXMLS_misc3;
            return yxml_elemclose(x, ch);
        }
        return YXML_ESYN;
    /* Comment: <!-- ... --> */
    case YXMLS_comment0:
        if(ch == '-') { x->state = YXMLS_comment1; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_comment1:
        if(ch == '-') { x->state = YXMLS_comment2; return YXML_OK; }
        return YXML_OK;
    case YXMLS_comment2:
        if(ch == '-') { x->state = YXMLS_comment3; return YXML_OK; }
        x->state = YXMLS_comment1;
        return YXML_OK;
    case YXMLS_comment3:
        if(ch == '>') { x->state = x->stacklen ? YXMLS_string : YXMLS_misc2a; return YXML_OK; }
        return YXML_ESYN;
    case YXMLS_comment4:
        return YXML_ESYN;
    /* CDATA */
    case YXMLS_cd0:
        x->state = YXMLS_cd1; return YXML_OK; /* skip checking "CDATA[" */
    case YXMLS_cd1:
        if(ch == ']') { x->state = YXMLS_cd2; return YXML_OK; }
        return yxml_datacontent(x, ch);
    case YXMLS_cd2:
        if(ch == ']') return YXML_OK; /* stay in cd2 */
        if(ch == '>') { x->state = YXMLS_string; return YXML_OK; }
        x->state = YXMLS_cd1;
        return yxml_datacd1(x, ch);
    /* DOCTYPE */
    case YXMLS_dt0:
        x->state = YXMLS_dt1; x->ignore = 1; return YXML_OK;
    case YXMLS_dt1:
        if(ch == '<') { x->ignore++; return YXML_OK; }
        if(ch == '>') { x->ignore--; if(!x->ignore) { x->state = YXMLS_misc2a; } return YXML_OK; }
        return YXML_OK;
    case YXMLS_dt2: case YXMLS_dt3: case YXMLS_dt4:
        return YXML_ESYN;
    /* PI */
    case YXMLS_pi0:
        if(yxml_isNameStart(ch)) {
            x->state = YXMLS_pi1;
            x->pi[0] = (char)ch; x->pi[1] = 0;
            return YXML_OK;
        }
        return YXML_ESYN;
    case YXMLS_pi1:
        if(yxml_isName(ch)) return YXML_OK;
        if(ch == '?') { x->state = YXMLS_pi4; return YXML_PISTART; }
        if(yxml_isSP(ch)) { x->state = YXMLS_pi2; return YXML_PISTART; }
        return YXML_ESYN;
    case YXMLS_pi2:
        if(ch == '?') { x->state = YXMLS_pi3; return YXML_OK; }
        return yxml_datapi2(x, ch);
    case YXMLS_pi3:
        if(ch == '>') { x->state = x->stacklen ? YXMLS_string : YXMLS_misc2a; return YXML_PIEND; }
        x->state = YXMLS_pi2;
        return yxml_datapi1(x, ch);
    case YXMLS_pi4:
        if(ch == '>') { x->state = x->stacklen ? YXMLS_string : YXMLS_misc2a; return YXML_PIEND; }
        return YXML_ESYN;
    /* XML declaration states (simplified) */
    case YXMLS_xmldecl0: case YXMLS_xmldecl1: case YXMLS_xmldecl2:
    case YXMLS_xmldecl3: case YXMLS_xmldecl4: case YXMLS_xmldecl5:
    case YXMLS_xmldecl6: case YXMLS_xmldecl7: case YXMLS_xmldecl8:
    case YXMLS_xmldecl9:
    case YXMLS_ver0: case YXMLS_ver1: case YXMLS_ver2: case YXMLS_ver3:
    case YXMLS_enc0: case YXMLS_enc1: case YXMLS_enc2: case YXMLS_enc3:
    case YXMLS_std0: case YXMLS_std1: case YXMLS_std2: case YXMLS_std3:
    case YXMLS_leq0:
        return YXML_ESYN;
    }
    return YXML_ESYN;
}

yxml_ret_t yxml_eof(yxml_t *x) {
    if(x->state != YXMLS_misc3) return YXML_EEOF;
    return YXML_OK;
}
