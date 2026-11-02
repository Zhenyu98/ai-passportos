#include "passport_settings.h"
#include "passport_json.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

passport_status_t passport_settings_parse(const char *body,size_t length,
        const passport_profile_t *base,passport_settings_update_t *out) {
    if(!base || !out) return PASSPORT_ERR_INVALID;
    cJSON *root=passport_json_parse(body,length);
    if(!root) return PASSPORT_ERR_INVALID;
    static const char *const names[]={"name","title","research","bio","homepage","epoch_ms"};
    passport_settings_update_t update={.profile=*base};
    bool valid=passport_json_key_mask(root,names,6)!=UINT_MAX;
    char *fields[]={update.profile.name,update.profile.title,update.profile.research,
                    update.profile.bio,update.profile.homepage};
    const size_t limits[]={PASSPORT_NAME_MAX,PASSPORT_TITLE_MAX,PASSPORT_RESEARCH_MAX,
                           PASSPORT_BIO_MAX,PASSPORT_HOMEPAGE_MAX};
    for(unsigned i=0;valid && i<5;i++) {
        const cJSON *value=cJSON_GetObjectItemCaseSensitive(root,names[i]);
        if(!value) continue;
        valid=cJSON_IsString(value) && value->valuestring && strlen(value->valuestring)<=limits[i];
        if(valid) memcpy(fields[i],value->valuestring,strlen(value->valuestring)+1);
    }
    const cJSON *epoch=cJSON_GetObjectItemCaseSensitive(root,"epoch_ms");
    if(valid && epoch && !cJSON_IsNull(epoch)) {
        valid=passport_json_integer(epoch,1577836800000.0,4102444799999.0);
        if(valid) {update.has_epoch=true;update.epoch=(int64_t)(epoch->valuedouble/1000.0);}
    }
    cJSON_Delete(root);
    if(!valid || !passport_profile_validate(&update.profile)) return PASSPORT_ERR_INVALID;
    *out=update;return PASSPORT_OK;
}
static void encode(uint32_t cp,char out[5]) {
    memset(out,0,5);
    if(cp<0x80) out[0]=(char)cp;
    else if(cp<0x800) {out[0]=(char)(0xc0|(cp>>6));out[1]=(char)(0x80|(cp&63));}
    else if(cp<0x10000) {
        out[0]=(char)(0xe0|(cp>>12));out[1]=(char)(0x80|((cp>>6)&63));out[2]=(char)(0x80|(cp&63));
    } else {
        out[0]=(char)(0xf0|(cp>>18));out[1]=(char)(0x80|((cp>>12)&63));
        out[2]=(char)(0x80|((cp>>6)&63));out[3]=(char)(0x80|(cp&63));
    }
}
void passport_settings_error(passport_status_t status,const passport_glyph_issue_t *issue,
                             char *out,size_t capacity) {
    if(!out || !capacity) return;
    if(status==PASSPORT_ERR_UNSUPPORTED && issue && issue->codepoint) {
        static const char *const labels[]={"姓名","学术身份","研究方向","简介","主页"};
        char character[5];encode(issue->codepoint,character);
        snprintf(out,capacity,"%s含有字库未收录的字符：%s（U+%04lX）。请更换字符或更新设备字库。",
            issue->field<5?labels[issue->field]:"资料",character,(unsigned long)issue->codepoint);
        return;
    }
    const char *message=status==PASSPORT_ERR_STORAGE?"设备暂时未能确认保存，请稍后重试。":
        status==PASSPORT_ERR_UNSUPPORTED?"资料含设备字库未收录的字符，请更新字库。":
        status==PASSPORT_ERR_CAPACITY?"内容过长，请缩短后重试。":"资料格式或时间无效，请检查输入。";
    snprintf(out,capacity,"%s",message);
}
