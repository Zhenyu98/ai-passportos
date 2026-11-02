#include "passport_config_auth.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    passport_config_auth_t auth;
    assert(passport_config_auth_init(&auth,true,"192.168.1.20","0123456789ab"));
    assert(!strcmp(auth.authorization,"Basic cGFzc3BvcnQ6MDEyMzQ1Njc4OWFi"));
    assert(!passport_config_auth_accept(&auth,"192.168.1.20",NULL,NULL));
    assert(!passport_config_auth_accept(&auth,"evil.example",NULL,auth.authorization));
    assert(!passport_config_auth_accept(&auth,"192.168.1.20","http://evil.example",auth.authorization));
    assert(!passport_config_auth_accept(&auth,"192.168.1.20","null",auth.authorization));
    assert(!passport_config_auth_accept(&auth,"192.168.1.20",NULL,"Basic invalid"));
    assert(passport_config_auth_accept(&auth,"192.168.1.20",NULL,auth.authorization));
    assert(passport_config_auth_accept(&auth,"192.168.1.20:80","http://192.168.1.20",auth.authorization));
    assert(passport_config_auth_accept(&auth,"192.168.1.20","http://192.168.1.20:80",auth.authorization));
    assert(!passport_config_auth_init(&auth,true,"bad/host","0123456789ab"));
    assert(!passport_config_auth_init(&auth,true,"192.168.1.20","short"));
    assert(passport_config_auth_init(&auth,false,NULL,NULL));
    assert(passport_config_auth_accept(&auth,NULL,NULL,NULL));
    puts("Configuration session authorization and origin policy: PASS");return 0;
}
