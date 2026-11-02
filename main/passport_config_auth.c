#include "passport_config_auth.h"
#include <stdio.h>
#include <string.h>

bool passport_config_auth_init(passport_config_auth_t *a,bool required,const char *ip,const char *code) {
    memset(a,0,sizeof *a);a->required=required;
    if(!required) return true;
    if(!ip || !code || !*ip || strlen(ip)>15 || strlen(code)!=12) return false;
    for(const char *p=ip;*p;p++) if((*p<'0'||*p>'9') && *p!='.') return false;
    for(const char *p=code;*p;p++) if(!(*p>='0'&&*p<='9') && !(*p>='a'&&*p<='f')) return false;
    snprintf(a->host,sizeof a->host,"%s",ip);
    snprintf(a->origin,sizeof a->origin,"http://%s",ip);
    char plain[22];snprintf(plain,sizeof plain,"passport:%s",code);
    static const char digits[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    memcpy(a->authorization,"Basic ",6);
    for(unsigned i=0,j=6;i<21;i+=3) {
        unsigned value=((unsigned char)plain[i]<<16)|((unsigned char)plain[i+1]<<8)|(unsigned char)plain[i+2];
        a->authorization[j++]=digits[value>>18];a->authorization[j++]=digits[value>>12&63];
        a->authorization[j++]=digits[value>>6&63];a->authorization[j++]=digits[value&63];
    }
    return true;
}
static bool same_endpoint(const char *got,const char *expected) {
    if(!got) return false;
    size_t n=strlen(expected);
    return !strcmp(got,expected) || (!strncmp(got,expected,n) && !strcmp(got+n,":80"));
}
bool passport_config_auth_accept(const passport_config_auth_t *a,const char *host,const char *origin,const char *authorization) {
    if(!a->required) return true;
    if(!a->host[0] || !a->authorization[0]) return false;
    if(!same_endpoint(host,a->host) || (origin && *origin && !same_endpoint(origin,a->origin)) || !authorization) return false;
    size_t n=strlen(a->authorization);
    if(strlen(authorization)!=n) return false;
    unsigned diff=0;
    for(size_t i=0;i<n;i++) diff|=(unsigned char)authorization[i]^(unsigned char)a->authorization[i];
    return diff==0;
}
