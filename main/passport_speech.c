#include "passport_speech.h"
#include <stdio.h>
#include <string.h>

bool passport_speech_config_valid(const passport_speech_config_t *c) {
    if(!c || !memchr(c->url,0,sizeof c->url) || !memchr(c->token,0,sizeof c->token)) return false;
    if(strlen(c->token)<16 || !c->url[0]) return false;
    for(const unsigned char *p=(const unsigned char *)c->token;*p;p++) if(*p<33 || *p>126) return false;
    for(const unsigned char *p=(const unsigned char *)c->url;*p;p++) if(*p<33 || *p>126 || *p=='#' || *p=='\\') return false;
    bool secure=!strncmp(c->url,"https://",8);
    if(!secure && strncmp(c->url,"http://",7)) return false;
    const char *host=c->url+(secure?8:7),*end=strpbrk(host,"/?:");
    size_t n=end?(size_t)(end-host):strlen(host);
    if(!n || n>100 || memchr(host,'@',n)) return false;
    /* Credentials in authority and ambiguous authorities are never accepted. */
    const char *path=strchr(host,'/');
    if(memchr(host,'@',path?(size_t)(path-host):strlen(host))) return false;
    if(!secure) {
        char address[101];memcpy(address,host,n);address[n]=0;
        unsigned a,b,d,e;char tail;
        if(sscanf(address,"%u.%u.%u.%u%c",&a,&b,&d,&e,&tail)!=4 || a>255 || b>255 || d>255 || e>255) return false;
        if(!(a==10 || (a==172 && b>=16 && b<=31) || (a==192 && b==168))) return false;
    }
    return true;
}
passport_speech_result_t passport_speech_play(const passport_speech_io_t *io,const char *text) {
    if(!io || !io->open || !io->read || !io->close || !io->begin || !io->write || !io->cancelled || !io->now_ms ||
       !text || !*text || strlen(text)>=PASSPORT_SPEECH_TEXT_MAX) return PASSPORT_SPEECH_FAILED;
    passport_speech_result_t result=PASSPORT_SPEECH_FAILED;
    uint8_t bytes[642];int16_t pcm[321];size_t carry=0,total=0;
    uint32_t started=io->now_ms(io->context),last=started;
    bool began=false;
    if(io->cancelled(io->context)) return PASSPORT_SPEECH_CANCELLED;
    if(!io->open(io->context,text)) {
        if(io->cancelled(io->context)) result=PASSPORT_SPEECH_CANCELLED;
        goto done;
    }
    for(;;) {
        if(io->cancelled(io->context)) {result=PASSPORT_SPEECH_CANCELLED;break;}
        uint32_t now=io->now_ms(io->context);
        if(now-started>150000u || now-last>10000u) break;
        int n=io->read(io->context,bytes+carry,640);
        if(io->cancelled(io->context)) {result=PASSPORT_SPEECH_CANCELLED;break;}
        if(n==-2) continue;
        if(n<0 || n>640) break;
        if(!n) {result=began && !carry?PASSPORT_SPEECH_OK:PASSPORT_SPEECH_FAILED;break;}
        last=io->now_ms(io->context);
        size_t available=(size_t)n+carry,frames=available/2;
        if(total+frames>PASSPORT_SPEECH_RATE*PASSPORT_SPEECH_SECONDS) break;
        for(size_t i=0;i<frames;i++) pcm[i]=(int16_t)((uint16_t)bytes[i*2]|(uint16_t)bytes[i*2+1]<<8);
        carry=available%2;if(carry) bytes[0]=bytes[available-1];
        if(!frames) continue;
        if(!began) {if(!io->begin(io->context)) break;began=true;}
        if(!io->write(io->context,pcm,frames)) break;
        total+=frames;
    }
done:
    io->close(io->context);
    return result;
}
