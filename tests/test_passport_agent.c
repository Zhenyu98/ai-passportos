#include "passport_agent_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    passport_agent_model_t a;
    passport_agent_model_init(&a);
    assert(a.state==PASSPORT_AGENT_UNAVAILABLE && a.generation==0);
    passport_agent_connection(&a,PASSPORT_AGENT_READY);
    uint32_t first=passport_agent_begin(&a);
    assert(first && a.state==PASSPORT_AGENT_RECORDING);
    assert(passport_agent_begin(&a)==0);
    passport_agent_leave(&a);
    assert(a.state==PASSPORT_AGENT_READY && a.generation!=first);
    passport_agent_event(&a,first,PASSPORT_AGENT_REPLY,"stale");
    assert(!a.reply[0]);
    uint32_t second=passport_agent_begin(&a);
    assert(second && second!=first);
    passport_agent_sent(&a);
    passport_agent_leave(&a);
    assert(a.state==PASSPORT_AGENT_WAITING);
    passport_agent_event(&a,second,PASSPORT_AGENT_REPLY,"研究任务已完成");
    assert(a.state==PASSPORT_AGENT_REPLY && !strcmp(a.reply,"研究任务已完成"));
    assert(passport_agent_save_note(&a,a.reply));
    assert(a.journal.count==1 && !strcmp(a.journal.notes[0],a.reply));
    for(unsigned i=0;i<12;i++) {
        char note[24];snprintf(note,sizeof note,"note %u",i);
        assert(passport_agent_save_note(&a,note));
    }
    assert(a.journal.count==PASSPORT_AGENT_NOTES && !strcmp(a.journal.notes[0],"note 11"));
    assert(!passport_agent_save_note(&a,""));
    assert(!passport_agent_save_note(&a,"\xc0\xaf"));
    char large[1200];memset(large,'A',sizeof large);large[sizeof large-1]=0;
    assert(!passport_agent_save_note(&a,large));
    passport_agent_event(&a,second,PASSPORT_AGENT_REPLY,large);
    assert(a.truncated && strlen(a.reply)==PASSPORT_AGENT_REPLY_MAX-1);
    char utf8[8];assert(passport_agent_copy(utf8,sizeof utf8,"甲乙丙"));
    assert(!strcmp(utf8,"甲乙"));
    assert(!passport_agent_copy(utf8,sizeof utf8,"\xed\xa0\x80"));
    passport_agent_journal_t saved=a.journal;
    passport_agent_model_init(&a);
    assert(passport_agent_restore(&a,&saved,sizeof saved));
    assert(a.journal.count==PASSPORT_AGENT_NOTES);
    saved.count=255;assert(!passport_agent_restore(&a,&saved,sizeof saved));
    saved.count=1;saved.version=99;assert(!passport_agent_restore(&a,&saved,sizeof saved));
    saved.version=1;memset(saved.notes[0],'x',sizeof saved.notes[0]);
    assert(!passport_agent_restore(&a,&saved,sizeof saved));
    assert(!passport_agent_restore(&a,&saved,sizeof saved-1));
    passport_agent_model_init(&a);passport_agent_connection(&a,PASSPORT_AGENT_READY);
    uint32_t pair_turn=passport_agent_begin(&a);passport_agent_sent(&a);
    passport_agent_event(&a,pair_turn,PASSPORT_AGENT_REPLY,"old result");
    passport_agent_connection(&a,PASSPORT_AGENT_CONFIRM);
    assert(a.state==PASSPORT_AGENT_CONFIRM);
    puts("Passport agent: PASS");
    return 0;
}
