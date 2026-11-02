#include "passport_muse_tools.h"
#include "passport_agent.h"
#include "passport_app.h"
#include <string.h>

static cJSON *result(bool ok,const char *message) {
    cJSON *r=cJSON_CreateObject();if(!r) return NULL;
    cJSON_AddBoolToObject(r,"ok",ok);
    if(ok) {
        cJSON *p=cJSON_AddObjectToObject(r,"payload");
        cJSON_AddBoolToObject(p,"accepted",true);
        cJSON_AddStringToObject(p,"status","queued; not yet applied or persisted");
    } else {
        cJSON *e=cJSON_AddObjectToObject(r,"error");
        cJSON_AddStringToObject(e,"code","passport_error");cJSON_AddStringToObject(e,"message",message);
    }
    return r;
}
static void command(cJSON *all,const char *name,const char *desc,const char *param,const char *type,const char *help) {
    cJSON *c=cJSON_AddObjectToObject(all,name);
    cJSON_AddStringToObject(c,"description",desc);
    cJSON *r=cJSON_AddObjectToObject(c,"required");cJSON_AddObjectToObject(c,"optional");
    if(param) {
        cJSON *p=cJSON_AddObjectToObject(r,param);
        cJSON_AddStringToObject(p,"type",type);cJSON_AddStringToObject(p,"description",help);
    }
}
void passport_muse_register(cJSON *all) {
    command(all,"passport.status","Read the badge battery percent, screen brightness and current page.",NULL,NULL,NULL);
    command(all,"passport.brightness","Queue a badge brightness change. Acceptance is not confirmation of persistence.",
        "percent","number","Exactly 30, 60 or 90 percent.");
    command(all,"passport.note","Queue saving a short note locally on the badge; keeps the newest eight notes. Acceptance does not confirm storage.",
        "text","string","Nonempty UTF-8 text, at most 255 bytes. Summarize longer material first.");
    command(all,"passport.open","Queue opening a named badge page. This does not cancel an already submitted cloud task.",
        "page","string","One of badge, agent, qr, menu, tasks, notes.");
}
cJSON *passport_muse_command(const char *name,cJSON *params) {
    if(!name || strncmp(name,"passport.",9)) return NULL;
    if(!strcmp(name,"passport.status")) {
        int battery;unsigned brightness;passport_page_t page;
        passport_app_device_status(&battery,&brightness,&page);
        cJSON *r=cJSON_CreateObject();if(!r)return NULL;
        cJSON_AddBoolToObject(r,"ok",true);cJSON *p=cJSON_AddObjectToObject(r,"payload");
        if(battery>=0) cJSON_AddNumberToObject(p,"battery_percent",battery);
        else cJSON_AddNullToObject(p,"battery_percent");
        cJSON_AddNumberToObject(p,"brightness_percent",brightness);
        static const char *pages[]={"badge","qr","menu","brightness","about","game","agent","tasks","notes"};
        cJSON_AddStringToObject(p,"page",page<=PASSPORT_PAGE_NOTES?pages[page]:"unknown");return r;
    }
    if(!strcmp(name,"passport.brightness")) {
        cJSON *v=cJSON_GetObjectItemCaseSensitive(params,"percent");
        if(!cJSON_IsNumber(v) || (v->valuedouble!=30 && v->valuedouble!=60 && v->valuedouble!=90))
            return result(false,"percent must be exactly 30, 60 or 90");
        return result(passport_app_remote_brightness((unsigned)v->valuedouble),"Device is busy or stopping; command not accepted");
    }
    if(!strcmp(name,"passport.note")) {
        cJSON *v=cJSON_GetObjectItemCaseSensitive(params,"text");char checked[PASSPORT_AGENT_NOTE_MAX];
        if(!cJSON_IsString(v) || !v->valuestring || !*v->valuestring || strlen(v->valuestring)>=sizeof checked ||
           !passport_agent_copy(checked,sizeof checked,v->valuestring))
            return result(false,"text must be nonempty UTF-8 of at most 255 bytes");
        return result(passport_agent_action(PASSPORT_AGENT_SAVE_TEXT,checked),"Note queue is busy; note not accepted");
    }
    if(!strcmp(name,"passport.open")) {
        cJSON *v=cJSON_GetObjectItemCaseSensitive(params,"page");
        if(cJSON_IsString(v) && v->valuestring) {
            static const struct {const char *name;passport_page_t page;} pages[]={
                {"badge",PASSPORT_PAGE_BADGE},{"agent",PASSPORT_PAGE_AGENT},{"qr",PASSPORT_PAGE_PICTURE},
                {"menu",PASSPORT_PAGE_MENU},{"tasks",PASSPORT_PAGE_TASKS},{"notes",PASSPORT_PAGE_NOTES}};
            for(unsigned i=0;i<sizeof pages/sizeof pages[0];i++)
                if(!strcmp(v->valuestring,pages[i].name)) return result(passport_app_remote_page(pages[i].page),"Device navigation is busy");
        }
        return result(false,"Unknown or missing page");
    }
    return result(false,"Unknown Passport command");
}
