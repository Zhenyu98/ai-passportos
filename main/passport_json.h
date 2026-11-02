#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
/* Bounded UTF-8 JSON; rejects escaped NUL, deep nesting and trailing input. */
cJSON *passport_json_parse(const char *text,size_t length);
unsigned passport_json_key_mask(const cJSON *,const char *const *,unsigned count);
bool passport_json_integer(const cJSON *,double minimum,double maximum);
