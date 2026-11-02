#include "passport_agent_model.h"
#include <string.h>

/* Validate the entire input, including the tail beyond the display bound. */
bool passport_agent_copy(char *out,size_t cap,const char *in) {
    if(!out || !cap || !in) return false;
    size_t used=0;bool full=false;
    while(*in) {
        const unsigned char *p=(const unsigned char *)in;
        size_t n;uint32_t cp;
        if(p[0]<0x80) {n=1;cp=p[0];}
        else if(p[0]>=0xc2 && p[0]<=0xdf) {n=2;cp=p[0]&31;}
        else if(p[0]>=0xe0 && p[0]<=0xef) {n=3;cp=p[0]&15;}
        else if(p[0]>=0xf0 && p[0]<=0xf4) {n=4;cp=p[0]&7;}
        else {out[0]=0;return false;}
        for(size_t i=1;i<n;i++) {
            if(!p[i] || (p[i]&0xc0)!=0x80) {out[0]=0;return false;}
            cp=(cp<<6)|(p[i]&63);
        }
        if((n==2 && cp<0x80) || (n==3 && cp<0x800) ||
           (n==4 && cp<0x10000) || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) {
            out[0]=0;return false;
        }
        if(!full && used+n<cap) {memcpy(out+used,in,n);used+=n;}
        else full=true;
        in+=n;
    }
    out[used]=0;return true;
}
void passport_agent_model_init(passport_agent_model_t *a) {
    memset(a,0,sizeof *a);a->journal.version=1;a->revision=1;
}
void passport_agent_connection(passport_agent_model_t *a,passport_agent_state_t state) {
    if(state==a->connection) return;
    a->connection=state;a->revision++;
    if(state==PASSPORT_AGENT_CONFIRM) {
        if(a->state==PASSPORT_AGENT_RECORDING || a->state==PASSPORT_AGENT_WAITING) a->generation++;
        if(!++a->confirmation) a->confirmation++;
        a->state=state;a->detail[0]=0;return;
    }
    if(a->state==PASSPORT_AGENT_RECORDING || a->state==PASSPORT_AGENT_WAITING) {
        if(state!=PASSPORT_AGENT_READY) {
            a->generation++;a->state=PASSPORT_AGENT_ERROR;
            passport_agent_copy(a->detail,sizeof a->detail,"连接中断，请重试");
        }
    } else if(a->state!=PASSPORT_AGENT_REPLY && a->state!=PASSPORT_AGENT_ERROR) a->state=state;
}
uint32_t passport_agent_begin(passport_agent_model_t *a) {
    if(a->connection!=PASSPORT_AGENT_READY || a->state==PASSPORT_AGENT_RECORDING ||
       a->state==PASSPORT_AGENT_WAITING) return 0;
    if(!++a->generation) a->generation++;
    a->state=PASSPORT_AGENT_RECORDING;a->revision++;a->truncated=false;
    a->heard[0]=a->reply[0]=a->detail[0]=0;
    return a->generation;
}
void passport_agent_sent(passport_agent_model_t *a) {
    if(a->state==PASSPORT_AGENT_RECORDING) {a->state=PASSPORT_AGENT_WAITING;a->revision++;}
}
void passport_agent_leave(passport_agent_model_t *a) {
    if(a->state!=PASSPORT_AGENT_RECORDING) return;
    a->generation++;a->state=a->connection;a->revision++;
}
void passport_agent_event(passport_agent_model_t *a,uint32_t gen,passport_agent_state_t state,const char *text) {
    if(!gen || gen!=a->generation || !text) return;
    if(state==PASSPORT_AGENT_REPLY) {
        if(!passport_agent_copy(a->reply,sizeof a->reply,text)) return;
        a->truncated=strlen(text)>=sizeof a->reply;
        memcpy(a->journal.result,a->reply,sizeof a->reply);
    } else if(state==PASSPORT_AGENT_ERROR) {
        if(!passport_agent_copy(a->detail,sizeof a->detail,text)) return;
    } else return;
    a->state=state;a->revision++;
}
bool passport_agent_save_note(passport_agent_model_t *a,const char *text) {
    if(!text || !*text || strlen(text)>=PASSPORT_AGENT_NOTE_MAX) return false;
    char checked[PASSPORT_AGENT_NOTE_MAX];
    if(!passport_agent_copy(checked,sizeof checked,text)) return false;
    memmove(a->journal.notes+1,a->journal.notes,sizeof a->journal.notes-sizeof a->journal.notes[0]);
    memset(a->journal.notes[0],0,sizeof a->journal.notes[0]);
    memcpy(a->journal.notes[0],checked,strlen(checked)+1);
    if(a->journal.count<PASSPORT_AGENT_NOTES) a->journal.count++;
    a->revision++;return true;
}
bool passport_agent_restore(passport_agent_model_t *a,const void *data,size_t bytes) {
    if(!data || bytes!=sizeof a->journal) return false;
    const passport_agent_journal_t *j=data;
    if(j->version!=1 || j->count>PASSPORT_AGENT_NOTES || !memchr(j->result,0,sizeof j->result)) return false;
    char checked[PASSPORT_AGENT_REPLY_MAX];
    if(!passport_agent_copy(checked,sizeof checked,j->result)) return false;
    for(unsigned i=0;i<PASSPORT_AGENT_NOTES;i++)
        if(!memchr(j->notes[i],0,sizeof j->notes[i]) ||
           !passport_agent_copy(checked,sizeof checked,j->notes[i])) return false;
    a->journal=*j;a->revision++;return true;
}
