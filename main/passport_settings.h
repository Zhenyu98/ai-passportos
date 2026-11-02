#pragma once
#include "passport_profile.h"
typedef struct {
    passport_profile_t profile;
    bool has_epoch;
    int64_t epoch;
} passport_settings_update_t;
passport_status_t passport_settings_parse(const char *,size_t,const passport_profile_t *,
                                          passport_settings_update_t *);
void passport_settings_error(passport_status_t,const passport_glyph_issue_t *,char *,size_t);
