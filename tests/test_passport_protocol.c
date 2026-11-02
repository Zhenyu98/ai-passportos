#include "passport_protocol.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static passport_status_t parse(const char *json, passport_request_t *out) {
    return passport_protocol_parse(json,strlen(json),out);
}
static void reject(const char *json) {
    passport_request_t out={.id=12345};
    assert(parse(json,&out)!=PASSPORT_OK);
    assert(out.id==12345);
}
static bool ascii_glyph(uint32_t cp, void *user) { (void)user; return cp>=32 && cp<127; }
int main(void) {
    passport_request_t out;
    assert(parse("{\"id\":1,\"op\":\"get\"}",&out)==PASSPORT_OK);
    assert(out.id==1 && out.op==PASSPORT_OP_GET);
    assert(parse("{\"id\":4294967295,\"op\":\"info\"}",&out)==PASSPORT_OK);
    assert(out.id==UINT32_MAX && out.op==PASSPORT_OP_INFO);
    const char *invalid[]={
        "{}","[]","{\"id\":true,\"op\":\"get\"}","{\"id\":-1,\"op\":\"get\"}",
        "{\"id\":1.5,\"op\":\"get\"}","{\"id\":4294967296,\"op\":\"get\"}",
        "{\"id\":1,\"id\":2,\"op\":\"get\"}","{\"id\":1,\"op\":\"nope\"}",
        "{\"id\":1,\"op\":\"get\",\"extra\":1}","{\"id\":1,\"op\":\"get\"} garbage",
        "{\"id\":1,\"op\":\"set\",\"profile\":{\"name\":\"Only\"}}",
        "{\"id\":1,\"op\":\"time\",\"epoch\":false}",
        "{\"id\":1,\"op\":\"time\",\"epoch\":1577836799}",
        "{\"id\":1,\"op\":\"time\",\"epoch\":4102444800}",
        "{\"id\":1,\"op\":\"get\",\"extra\":[[[[[[[[0]]]]]]]]}"
    };
    for (unsigned i=0;i<sizeof invalid/sizeof invalid[0];++i) reject(invalid[i]);
    assert(parse("{\"id\":9,\"op\":\"time\",\"epoch\":4102444799}",&out)==PASSPORT_OK);
    assert(out.epoch==INT64_C(4102444799));
    const char *valid_set="{\"id\":2,\"op\":\"set\",\"profile\":{\"name\":\"Alice\",\"title\":\"\",\"research\":\"AI\",\"bio\":\"line\\nline\",\"homepage\":\"https://example.org\"}}";
    assert(parse(valid_set,&out)==PASSPORT_OK && out.op==PASSPORT_OP_SET);
    assert(strcmp(out.profile.name,"Alice")==0);
    assert(passport_profile_glyphs_supported(&out.profile,ascii_glyph,NULL));
    strcpy(out.profile.name,"\xe4\xb8\xad");
    assert(!passport_profile_glyphs_supported(&out.profile,ascii_glyph,NULL));
    reject("{\"id\":2,\"op\":\"set\",\"profile\":{\"name\":\"\\u0000Alice\",\"title\":\"\",\"research\":\"\",\"bio\":\"\",\"homepage\":\"\"}}");
    reject("{\"id\":2,\"op\":\"set\",\"profile\":{\"name\":\"A\",\"name\":\"B\",\"title\":\"\",\"research\":\"\",\"bio\":\"\",\"homepage\":\"\"}}");
    const char *literal="{\"id\":2,\"op\":\"set\",\"profile\":{\"name\":\"\\\\u0000\",\"title\":\"\",\"research\":\"\",\"bio\":\"\",\"homepage\":\"\"}}";
    assert(parse(literal,&out)==PASSPORT_OK && strcmp(out.profile.name,"\\u0000")==0);
    char oversized[PASSPORT_LINE_MAX+1]; memset(oversized,' ',sizeof oversized);
    assert(passport_protocol_parse(oversized,sizeof oversized,&out)==PASSPORT_ERR_CAPACITY);

    passport_line_reader_t reader={0}; const char *line=NULL; size_t length=0; unsigned overflows=0;
    for(unsigned i=0;i<PASSPORT_LINE_MAX+10;++i)
        if(passport_line_feed(&reader,'x',&line,&length)==PASSPORT_LINE_OVERFLOW) ++overflows;
    assert(overflows==1);
    assert(passport_line_feed(&reader,'\n',&line,&length)==PASSPORT_LINE_NONE);
    const char *next="{\"id\":7,\"op\":\"get\"}\r\n";
    passport_line_event_t event=PASSPORT_LINE_NONE;
    for(size_t i=0;i<strlen(next);++i) event=passport_line_feed(&reader,(uint8_t)next[i],&line,&length);
    assert(event==PASSPORT_LINE_READY);
    assert(passport_protocol_parse(line,length,&out)==PASSPORT_OK && out.id==7);

    passport_reply_t reply={.id=17,.op=PASSPORT_OP_GET,.status=PASSPORT_OK,.record={.schema=1,.brightness=60}};
    strcpy(reply.record.profile.name,"Alice");
    char response[PASSPORT_LINE_MAX]; size_t written=0;
    assert(passport_protocol_reply(&reply,response,sizeof response,&written)==PASSPORT_OK);
    assert(strncmp(response,PASSPORT_RESPONSE_PREFIX,strlen(PASSPORT_RESPONSE_PREFIX))==0 && response[written-1]=='\n');
    cJSON *json=cJSON_Parse(response+strlen(PASSPORT_RESPONSE_PREFIX)); assert(json);
    assert(cJSON_GetObjectItemCaseSensitive(json,"id")->valuedouble==17);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json,"ok"))); cJSON_Delete(json);
    reply.status=PASSPORT_ERR_STORAGE; strcpy(reply.record.profile.name,"DoNotEchoPrivateValues");
    assert(passport_protocol_reply(&reply,response,sizeof response,&written)==PASSPORT_OK);
    assert(strstr(response,"storage") && !strstr(response,"DoNotEchoPrivateValues"));
    static uint32_t large_inventory[7000];
    for(unsigned i=0;i<7000;i++) large_inventory[i]=0x4e00+i;
    reply=(passport_reply_t){.id=19,.op=PASSPORT_OP_INFO,.status=PASSPORT_OK,
        .glyphs=large_inventory,.glyph_count=7000};
    assert(passport_protocol_reply(&reply,response,sizeof response,&written)==PASSPORT_OK);
    assert(written<1024 && strstr(response,"\"glyphs_complete\":false"));
    for(unsigned i=0;i<PASSPORT_GLYPH_LIST_MAX;i++) large_inventory[i]=0x10ffff;
    reply.glyph_count=PASSPORT_GLYPH_LIST_MAX; /* worst case still fits one line */
    assert(passport_protocol_reply(&reply,response,sizeof response,&written)==PASSPORT_OK);
    assert(strstr(response,"\"glyphs_complete\":true"));
    reply.op=PASSPORT_OP_GET;reply.record.schema=1;reply.record.brightness=60;
    assert(passport_protocol_reply(&reply,response,sizeof response,&written)==PASSPORT_OK);
    assert(passport_protocol_reply(&reply,response,8,&written)==PASSPORT_ERR_CAPACITY && written==0);
    puts("Passport protocol and framing: PASS");
    return 0;
}
