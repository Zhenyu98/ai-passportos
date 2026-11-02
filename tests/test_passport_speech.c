#include "passport_speech.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {unsigned pos,writes,closed,opens,began,clock,limit;bool cancel,fail;int16_t pcm[32];unsigned samples;} fake_t;
static bool open_source(void *p,const char *text) {fake_t *f=p;f->opens++;assert(!strcmp(text,"你好"));return !f->fail;}
static int read_source(void *p,uint8_t *out,size_t cap) {
    fake_t *f=p;f->clock+=20;assert(cap>=7);
    static const uint8_t data[]={1,0,255,127,0,128,254,255,42,0};
    if(f->pos>=f->limit)return 0;
    unsigned n=f->pos==0?3:7;
    if(n>f->limit-f->pos)n=f->limit-f->pos;
    memcpy(out,data+f->pos,n);f->pos+=n;return (int)n;
}
static void close_source(void *p) {((fake_t *)p)->closed++;}
static bool begin_audio(void *p) {((fake_t *)p)->began++;return true;}
static bool write_audio(void *p,const int16_t *pcm,size_t frames) {
    fake_t *f=p;assert(f->samples+frames<=32);memcpy(f->pcm+f->samples,pcm,frames*2);f->samples+=(unsigned)frames;f->writes++;return !f->fail;
}
static bool cancelled(void *p) {fake_t *f=p;return f->cancel && f->writes>0;}
static uint32_t now(void *p) {return ((fake_t *)p)->clock;}
static int endless(void *p,uint8_t *out,size_t cap) {(void)p;memset(out,0,cap);return (int)cap;}
static int stalled(void *p,uint8_t *out,size_t cap) {(void)out;(void)cap;((fake_t *)p)->clock+=1000;return -2;}
static bool count_audio(void *p,const int16_t *pcm,size_t frames) {
    (void)pcm;fake_t *f=p;f->samples+=(unsigned)frames;
    assert(f->samples<=PASSPORT_SPEECH_RATE*PASSPORT_SPEECH_SECONDS);return true;
}
int main(void) {
    fake_t f={.limit=10};
    passport_speech_io_t io={.context=&f,.open=open_source,.read=read_source,.close=close_source,
        .begin=begin_audio,.write=write_audio,.cancelled=cancelled,.now_ms=now};
    assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_OK);
    assert(f.samples==5 && f.began==1 && f.closed==1);
    assert(f.pcm[0]==1 && f.pcm[1]==32767 && f.pcm[2]==-32768 && f.pcm[3]==-2 && f.pcm[4]==42);
    f=(fake_t){.limit=9};assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_FAILED && f.closed==1);
    f=(fake_t){.limit=10,.cancel=true};assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_CANCELLED && f.samples==1 && f.closed==1);
    f=(fake_t){.fail=true};assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_FAILED && f.closed==1 && !f.began);
    f=(fake_t){0};assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_FAILED && !f.began);
    f=(fake_t){0};io.read=endless;
    /* An output cap is checked before another oversized block reaches the sink. */
    io.write=count_audio;assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_FAILED);
    assert(f.samples==PASSPORT_SPEECH_RATE*PASSPORT_SPEECH_SECONDS && f.closed==1);
    io.write=write_audio;io.read=stalled;f=(fake_t){0};
    assert(passport_speech_play(&io,"你好")==PASSPORT_SPEECH_FAILED && f.closed==1 && !f.began);
    passport_speech_config_t config={.url="https://speech.example/tts",.token="0123456789abcdef"};
    assert(passport_speech_config_valid(&config));
    strcpy(config.url,"http://192.168.1.4:8765/tts");assert(passport_speech_config_valid(&config));
    strcpy(config.url,"http://speech.example/tts");assert(!passport_speech_config_valid(&config));
    strcpy(config.url,"https://user:pass@example.org/");assert(!passport_speech_config_valid(&config));
    strcpy(config.url,"https://example.org/\r\nHeader");assert(!passport_speech_config_valid(&config));
    strcpy(config.url,"https://speech.example/tts");strcpy(config.token,"bad\nheader0123456");assert(!passport_speech_config_valid(&config));
    puts("Bounded speech PCM, odd chunks, cancellation, timeout and configuration: PASS");
}
