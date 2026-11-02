#include "passport_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool ascii(uint32_t cp,void *user) {(void)user;return cp>=32 && cp<127;}
static void parse_failure(const char *body,const passport_profile_t *base) {
    passport_settings_update_t result={.epoch=123};
    assert(passport_settings_parse(body,strlen(body),base,&result)!=PASSPORT_OK);
    assert(result.epoch==123);
}
int main(void) {
    passport_profile_t base={0};strcpy(base.name,"Old");strcpy(base.homepage,"https://example.org/");
    passport_settings_update_t update;
    const char *body="{\"name\":\"Alice\",\"epoch_ms\":1761004800123}";
    assert(passport_settings_parse(body,strlen(body),&base,&update)==PASSPORT_OK);
    assert(!strcmp(update.profile.name,"Alice") && !strcmp(update.profile.homepage,base.homepage));
    assert(update.has_epoch && update.epoch==1761004800);
    const char *bad[]={"{\"name\":\"A\",\"name\":\"B\"}","{\"name\":\"A\\u0000B\"}",
        "{\"name\":\"A\"} trailing","{\"unknown\":\"x\"}","{\"name\":123}",
        "{\"name\":\"ok\",\"epoch_ms\":1e100}","{\"name\":\"ok\",\"epoch_ms\":true}",
        "{\"name\":\"ok\",\"epoch_ms\":4102444800000}"};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++) parse_failure(bad[i],&base);
    passport_glyph_issue_t issue;
    strcpy(base.name,"\xe5\xbc\xa0\xe4\xb8\x89");
    assert(passport_profile_check(&base,ascii,NULL,&issue)==PASSPORT_ERR_UNSUPPORTED);
    assert(issue.field==0 && issue.codepoint==0x5f20);
    char message[256];
    passport_settings_error(PASSPORT_ERR_UNSUPPORTED,&issue,message,sizeof message);
    assert(strstr(message,"\xe5\xbc\xa0") && strstr(message,"U+5F20") && !strstr(message,"unsupported"));
    assert(passport_settings_parse("{\"name\":\"Alice\"}",16,&base,&update)==PASSPORT_OK);
    puts("Settings parsing, partial updates and actionable glyph errors: PASS");
}
