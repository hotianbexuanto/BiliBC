/* i18n.h - Internationalization (Chinese/English) */
#ifndef I18N_H
#define I18N_H

typedef enum {
    LANG_EN = 0,
    LANG_ZH = 1,
    LANG_COUNT
} Language;

/* Initialize i18n system */
void i18n_init(Language lang);

/* Set current language */
void i18n_set_language(Language lang);

/* Get current language */
Language i18n_get_language(void);

/* Get translated string by key. Returns key itself if not found. */
const char *i18n_get(const char *key);

/* Convenience macro */
#define _(key) i18n_get(key)

#endif /* I18N_H */
