#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PASSPORT_NAME_MAX 64
#define PASSPORT_TITLE_MAX 96
#define PASSPORT_RESEARCH_MAX 192
#define PASSPORT_BIO_MAX 512
#define PASSPORT_HOMEPAGE_MAX 128

typedef enum {
    PASSPORT_OK = 0, PASSPORT_ERR_INVALID, PASSPORT_ERR_NOT_FOUND,
    PASSPORT_ERR_CORRUPT, PASSPORT_ERR_STORAGE, PASSPORT_ERR_CAPACITY,
    PASSPORT_ERR_UNSUPPORTED
} passport_status_t;

typedef struct {
    char name[PASSPORT_NAME_MAX + 1];
    char title[PASSPORT_TITLE_MAX + 1];
    char research[PASSPORT_RESEARCH_MAX + 1];
    char bio[PASSPORT_BIO_MAX + 1];
    char homepage[PASSPORT_HOMEPAGE_MAX + 1];
} passport_profile_t;
typedef bool (*passport_glyph_fn)(uint32_t codepoint, void *user);
typedef struct {unsigned field;uint32_t codepoint;} passport_glyph_issue_t;
passport_status_t passport_profile_check(const passport_profile_t *,passport_glyph_fn,void *,
                                         passport_glyph_issue_t *);

bool passport_utf8_next(const char *text, size_t length, size_t *offset, uint32_t *codepoint);
bool passport_homepage_validate(const char *url);
bool passport_profile_validate(const passport_profile_t *profile);
