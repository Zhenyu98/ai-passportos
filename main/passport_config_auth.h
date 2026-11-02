#pragma once
#include <stdbool.h>
typedef struct {
    bool required;
    char host[24],origin[32],authorization[64];
} passport_config_auth_t;
bool passport_config_auth_init(passport_config_auth_t *,bool,const char *ip,const char *code);
bool passport_config_auth_accept(const passport_config_auth_t *,const char *host,const char *origin,const char *authorization);
