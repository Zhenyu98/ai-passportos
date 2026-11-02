#pragma once
#include "passport_store.h"

/* A full set request is about 2 KB; the Muse firmware needs the internal RAM. */
#define PASSPORT_LINE_MAX 4096
/* Largest glyph list info sends in full: 400 seven-digit entries fit one line. */
#define PASSPORT_GLYPH_LIST_MAX 400
#define PASSPORT_RESPONSE_PREFIX "PASSPORT/1 "
typedef enum {
    PASSPORT_OP_INFO, PASSPORT_OP_GET, PASSPORT_OP_SET, PASSPORT_OP_TIME,
    PASSPORT_OP_BRIGHTNESS /* Internal queue operation; never accepted from USB JSON. */
} passport_op_t;
typedef struct {
    uint32_t id;
    passport_op_t op;
    passport_profile_t profile;
    int64_t epoch;
    uint8_t brightness;
} passport_request_t;
typedef struct {
    uint32_t id;
    passport_op_t op;
    passport_status_t status;
    passport_record_t record;
    bool time_valid;
    int64_t epoch;
    const uint32_t *glyphs;
    size_t glyph_count;
    const char *version;
} passport_reply_t;
typedef passport_status_t (*passport_service_fn)(const passport_request_t *, passport_reply_t *, void *user);

passport_status_t passport_protocol_parse(const char *, size_t, passport_request_t *);
passport_status_t passport_protocol_reply(const passport_reply_t *, char *, size_t, size_t *written);
bool passport_profile_glyphs_supported(const passport_profile_t *, passport_glyph_fn, void *);
const char *passport_status_name(passport_status_t);

typedef struct { char bytes[PASSPORT_LINE_MAX+1]; size_t used; bool discard; } passport_line_reader_t;
typedef enum { PASSPORT_LINE_NONE, PASSPORT_LINE_READY, PASSPORT_LINE_OVERFLOW } passport_line_event_t;
/* READY borrows bytes until the next feed. Parse before enqueuing an owned request. */
passport_line_event_t passport_line_feed(passport_line_reader_t *, uint8_t, const char **, size_t *);
