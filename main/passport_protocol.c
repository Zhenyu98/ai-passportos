#include "passport_protocol.h"
#include "cJSON.h"
#include "passport_json.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static bool read_profile(const cJSON *json, passport_profile_t *p) {
    static const char *const names[]={"name","title","research","bio","homepage"};
    if (passport_json_key_mask(json,names,5)!=31u) return false;
    char *fields[]={p->name,p->title,p->research,p->bio,p->homepage};
    const size_t limits[]={PASSPORT_NAME_MAX,PASSPORT_TITLE_MAX,PASSPORT_RESEARCH_MAX,PASSPORT_BIO_MAX,PASSPORT_HOMEPAGE_MAX};
    for (unsigned i=0;i<5;++i) {
        const cJSON *item=cJSON_GetObjectItemCaseSensitive(json,names[i]);
        if (!cJSON_IsString(item) || !item->valuestring) return false;
        size_t n=strlen(item->valuestring);
        if (n>limits[i]) return false;
        memcpy(fields[i],item->valuestring,n+1);
    }
    return passport_profile_validate(p);
}

passport_status_t passport_protocol_parse(const char *line, size_t length, passport_request_t *out) {
    if (!line || !out || !length) return PASSPORT_ERR_INVALID;
    if (length>PASSPORT_LINE_MAX) return PASSPORT_ERR_CAPACITY;
    cJSON *root=passport_json_parse(line,length);
    if (!root) return PASSPORT_ERR_INVALID;
    static const char *const keys[]={"id","op","profile","epoch"};
    unsigned mask=passport_json_key_mask(root,keys,4);
    const cJSON *id=cJSON_GetObjectItemCaseSensitive(root,"id");
    const cJSON *op=cJSON_GetObjectItemCaseSensitive(root,"op");
    passport_request_t request={0};
    bool valid=passport_json_integer(id,0,UINT32_MAX)
        && cJSON_IsString(op) && op->valuestring;
    if (valid) {
        request.id=(uint32_t)id->valuedouble;
        if (strcmp(op->valuestring,"info")==0 && mask==3) request.op=PASSPORT_OP_INFO;
        else if (strcmp(op->valuestring,"get")==0 && mask==3) request.op=PASSPORT_OP_GET;
        else if (strcmp(op->valuestring,"set")==0 && mask==7) {
            request.op=PASSPORT_OP_SET;
            valid=read_profile(cJSON_GetObjectItemCaseSensitive(root,"profile"),&request.profile);
        } else if (strcmp(op->valuestring,"time")==0 && mask==11) {
            request.op=PASSPORT_OP_TIME;
            const cJSON *epoch=cJSON_GetObjectItemCaseSensitive(root,"epoch");
            valid=passport_json_integer(epoch,1577836800.0,4102444799.0);
            if (valid) request.epoch=(int64_t)epoch->valuedouble;
        } else valid=false;
    }
    cJSON_Delete(root);
    if (!valid) return PASSPORT_ERR_INVALID;
    *out=request;
    return PASSPORT_OK;
}

const char *passport_status_name(passport_status_t status) {
    static const char *const names[]={"ok","invalid","not_found","corrupt","storage","capacity","unsupported"};
    unsigned i=(unsigned)status;
    return i<sizeof names/sizeof names[0] ? names[i] : "invalid";
}
bool passport_profile_glyphs_supported(const passport_profile_t *p, passport_glyph_fn has, void *user) {
    return passport_profile_check(p,has,user,NULL)==PASSPORT_OK;
}

static cJSON *profile_json(const passport_profile_t *p) {
    cJSON *object=cJSON_CreateObject();
    if (!object) return NULL;
    const char *names[]={"name","title","research","bio","homepage"};
    const char *fields[]={p->name,p->title,p->research,p->bio,p->homepage};
    for (unsigned i=0;i<5;++i) if (!cJSON_AddStringToObject(object,names[i],fields[i])) {
        cJSON_Delete(object); return NULL;
    }
    return object;
}
passport_status_t passport_protocol_reply(const passport_reply_t *r, char *out, size_t capacity, size_t *written) {
    if (written) *written=0;
    if (!r || !out || !written) return PASSPORT_ERR_INVALID;
    const size_t prefix=strlen(PASSPORT_RESPONSE_PREFIX);
    if (capacity<prefix+8 || capacity>INT_MAX) return PASSPORT_ERR_CAPACITY;
    out[0]=0;
    if (r->status==PASSPORT_OK && r->op==PASSPORT_OP_GET && !passport_profile_validate(&r->record.profile))
        return PASSPORT_ERR_INVALID;
    if (r->glyph_count && !r->glyphs) return PASSPORT_ERR_CAPACITY;
    cJSON *root=cJSON_CreateObject();
    if (!root) return PASSPORT_ERR_CAPACITY;
    bool ok=cJSON_AddNumberToObject(root,"id",r->id) && cJSON_AddBoolToObject(root,"ok",r->status==PASSPORT_OK);
    if (r->status!=PASSPORT_OK) {
        ok=ok && cJSON_AddStringToObject(root,"error",passport_status_name(r->status));
    } else if (r->op==PASSPORT_OP_INFO) {
        ok=ok && cJSON_AddNumberToObject(root,"schema",1)
            && cJSON_AddStringToObject(root,"version",r->version ? r->version : "0.1");
        cJSON *glyphs=cJSON_AddArrayToObject(root,"glyphs");
        cJSON *capabilities=cJSON_AddArrayToObject(root,"capabilities");
        bool complete=r->glyph_count<=PASSPORT_GLYPH_LIST_MAX;
        ok=ok && glyphs && capabilities && cJSON_AddBoolToObject(root,"glyphs_complete",complete);
        for (size_t i=0;ok && complete && i<r->glyph_count;++i) {
            cJSON *item=cJSON_CreateNumber(r->glyphs[i]);
            if (!item || !cJSON_AddItemToArray(glyphs,item)) { cJSON_Delete(item); ok=false; }
        }
        const char *ops[]={"info","get","set","time"};
        for (unsigned i=0;ok && i<4;++i) {
            cJSON *item=cJSON_CreateString(ops[i]);
            if (!item || !cJSON_AddItemToArray(capabilities,item)) { cJSON_Delete(item); ok=false; }
        }
    } else if (r->op==PASSPORT_OP_GET) {
        cJSON *profile=profile_json(&r->record.profile);
        if (!profile || !cJSON_AddItemToObject(root,"profile",profile)) {
            cJSON_Delete(profile); ok=false;
        }
        ok=ok && cJSON_AddNumberToObject(root,"brightness",r->record.brightness)
            && cJSON_AddBoolToObject(root,"time_valid",r->time_valid);
        if (r->time_valid) ok=ok && cJSON_AddNumberToObject(root,"epoch",(double)r->epoch);
    }
    if (ok) {
        memcpy(out,PASSPORT_RESPONSE_PREFIX,prefix);
        ok=cJSON_PrintPreallocated(root,out+prefix,(int)(capacity-prefix-1),false);
    }
    cJSON_Delete(root);
    if (!ok) { out[0]=0; return PASSPORT_ERR_CAPACITY; }
    size_t length=strlen(out);
    if (length+2>capacity) { out[0]=0; return PASSPORT_ERR_CAPACITY; }
    out[length]='\n'; out[length+1]=0; *written=length+1;
    return PASSPORT_OK;
}

passport_line_event_t passport_line_feed(passport_line_reader_t *r, uint8_t byte,
                                        const char **line, size_t *length) {
    if (!r || !line || !length) return PASSPORT_LINE_NONE;
    *line=NULL; *length=0;
    if (byte=='\n') {
        if (r->discard) { r->discard=false; r->used=0; return PASSPORT_LINE_NONE; }
        size_t size=r->used;
        if (size && r->bytes[size-1]=='\r') --size;
        r->used=0; r->bytes[size]=0;
        if (!size) return PASSPORT_LINE_NONE;
        *line=r->bytes; *length=size;
        return PASSPORT_LINE_READY;
    }
    if (r->discard) return PASSPORT_LINE_NONE;
    if (r->used==PASSPORT_LINE_MAX) {
        r->discard=true; r->used=0;
        return PASSPORT_LINE_OVERFLOW;
    }
    r->bytes[r->used++]=(char)byte;
    return PASSPORT_LINE_NONE;
}
