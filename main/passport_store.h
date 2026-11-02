#pragma once
#include "passport_profile.h"

typedef struct {
    uint16_t schema;
    uint8_t brightness;
    passport_profile_t profile;
} passport_record_t;

/* write atomically replaces one record; failed writes must retain the old value. */
typedef struct {
    void *ctx;
    passport_status_t (*read)(void *ctx, uint8_t *buffer, size_t *inout_length);
    passport_status_t (*write)(void *ctx, const uint8_t *buffer, size_t length);
} passport_store_backend_t;

passport_status_t passport_store_load(const passport_store_backend_t *, passport_record_t *out);
passport_status_t passport_store_save(const passport_store_backend_t *, passport_record_t *current,
                                      const passport_record_t *candidate);
