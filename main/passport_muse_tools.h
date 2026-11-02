#pragma once
#include "cJSON.h"
#ifdef __cplusplus
extern "C" {
#endif
void passport_muse_register(cJSON *commands);
/* NULL means this command belongs to the upstream SDK. */
cJSON *passport_muse_command(const char *name,cJSON *params);
#ifdef __cplusplus
}
#endif
