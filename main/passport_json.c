#include "passport_json.h"
#include "passport_profile.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static bool is_hex(char c) {
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}
static bool raw_json_valid(const char *line, size_t length) {
    bool quoted=false;
    unsigned depth=0, separators=0;
    size_t at=0;
    while (at<length) {
        uint32_t cp;
        if (!passport_utf8_next(line,length,&at,&cp) || cp==0) return false;
    }
    for (size_t i=0;i<length;++i) {
        unsigned char c=(unsigned char)line[i];
        if (quoted) {
            if (c<0x20) return false;
            if (c=='"') quoted=false;
            else if (c=='\\') {
                if (++i>=length) return false;
                if (line[i]=='u') {
                    if (length-i<5) return false;
                    bool zero=true;
                    for (unsigned k=1;k<=4;++k) {
                        if (!is_hex(line[i+k])) return false;
                        if (line[i+k]!='0') zero=false;
                    }
                    if (zero) return false;
                    i+=4;
                }
            }
        } else if (c=='"') quoted=true;
        else if (c=='{' || c=='[') { if (++depth>4) return false; }
        else if (c=='}' || c==']') { if (!depth) return false; --depth; }
        else if (c==':' || c==',') { if (++separators>32) return false; }
    }
    return !quoted && depth==0;
}

unsigned passport_json_key_mask(const cJSON *object, const char *const *allowed, unsigned count) {
    if (!cJSON_IsObject(object)) return UINT_MAX;
    unsigned mask=0;
    for (const cJSON *item=object->child;item;item=item->next) {
        unsigned i;
        for (i=0;i<count;++i) if (item->string && strcmp(item->string,allowed[i])==0) break;
        if (i==count || (mask & (1u<<i))) return UINT_MAX;
        mask|=1u<<i;
    }
    return mask;
}
bool passport_json_integer(const cJSON *item, double minimum, double maximum) {
    return cJSON_IsNumber(item) && isfinite(item->valuedouble)
        && item->valuedouble>=minimum && item->valuedouble<=maximum
        && floor(item->valuedouble)==item->valuedouble;
}
cJSON *passport_json_parse(const char *text,size_t length) {
    if(!text || !length || length>8192 || !raw_json_valid(text,length)) return NULL;
    const char *end=NULL;
    cJSON *root=cJSON_ParseWithLengthOpts(text,length,&end,false);
    if(!root) return NULL;
    while(end<text+length && (*end==' ' || *end=='\t' || *end=='\r' || *end=='\n')) ++end;
    if(end!=text+length || !cJSON_IsObject(root)) {cJSON_Delete(root);return NULL;}
    return root;
}
