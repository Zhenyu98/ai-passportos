#include "passport_muse_tools.h"
#include "passport_app.h"
#include "passport_agent.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned writes;
static bool accept=true;
bool passport_app_remote_brightness(unsigned v) {writes++;return accept && passport_brightness_valid(v);}
bool passport_app_remote_page(passport_page_t p) {writes++;return accept && p==PASSPORT_PAGE_AGENT;}
void passport_app_device_status(int *b,unsigned *v,passport_page_t *p) {*b=70;*v=60;*p=PASSPORT_PAGE_BADGE;}
bool passport_agent_action(unsigned a,const char *t) {writes++;return accept && a==PASSPORT_AGENT_SAVE_TEXT && t && *t;}
static bool invoke(const char *command,const char *json) {
    cJSON *params=json?cJSON_Parse(json):NULL;
    cJSON *r=passport_muse_command(command,params);assert(r);
    bool ok=cJSON_IsTrue(cJSON_GetObjectItem(r,"ok"));cJSON_Delete(params);cJSON_Delete(r);return ok;
}
int main(void) {
    cJSON *commands=cJSON_CreateObject();passport_muse_register(commands);
    assert(cJSON_GetArraySize(commands)==4);cJSON_Delete(commands);
    assert(!passport_muse_command("device.health",NULL));
    assert(!invoke("passport.brightness",NULL));
    assert(!invoke("passport.brightness","{\"percent\":60.5}"));
    assert(!invoke("passport.brightness","{\"percent\":6000000000}"));
    assert(!invoke("passport.brightness","{\"percent\":\"60\"}"));
    assert(!invoke("passport.brightness","{\"percent\":0}"));
    assert(!invoke("passport.brightness","{\"percent\":-10}"));
    assert(!invoke("passport.brightness","{\"percent\":55}"));
    assert(!invoke("passport.brightness","{\"percent\":110}"));
    assert(!invoke("passport.open","{\"page\":\"shell\"}"));
    assert(!invoke("passport.note","{\"text\":\"\"}"));
    assert(!writes);
    assert(invoke("passport.status",NULL));
    assert(invoke("passport.brightness","{\"percent\":60}"));
    assert(invoke("passport.brightness","{\"percent\":10}"));
    assert(invoke("passport.brightness","{\"percent\":100}"));
    assert(invoke("passport.open","{\"page\":\"agent\"}"));
    assert(invoke("passport.note","{\"text\":\"会议笔记\"}"));
    accept=false;assert(!invoke("passport.note","{\"text\":\"重试\"}"));
    assert(!invoke("passport.unknown",NULL));
    puts("Muse device tools: PASS");return 0;
}
