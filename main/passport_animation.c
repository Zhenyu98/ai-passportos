#include "passport_animation.h"
#include <string.h>

typedef struct {uint8_t frame; uint16_t duration;} keyframe_t;
static const keyframe_t idle[] = {{0,0},{1,700},{2,90},{3,100},{4,90},{0,80}};
/* Both buttons are below/right of the left-column avatar: near target uses a
 * small downward pitch; the far QR target uses the right/down gaze family. */
static const keyframe_t near[] = {{13,150},{0,150}};
static const keyframe_t far[] = {{10,100},{11,200},{10,100},{0,100}};
static const keyframe_t nod[] = {{13,80},{14,100},{15,80},{16,100},{0,80}};

static void rest(passport_animation_t *p, uint32_t now) {
    p->random = p->random * 1664525u + 1013904223u;
    p->dwell = (uint16_t)(2000u + p->random % 4001u);
    p->state = 0; p->step = 0; p->began = now;
}
void passport_animation_show(passport_animation_t *p, uint32_t now) {
    if(!p) return;
    memset(p,0,sizeof *p); p->visible=true; p->random=now^0x50415353u;
    rest(p,now);
}
void passport_animation_hide(passport_animation_t *p) {
    if(p) memset(p,0,sizeof *p);
}
void passport_animation_event(passport_animation_t *p,passport_animation_event_t event,uint32_t now) {
    if(!p || !p->visible || event<PASSPORT_ANIMATION_FOCUS_AGENT || event>PASSPORT_ANIMATION_NOD) return;
    p->state=(uint8_t)event; p->step=0; p->began=now;
}
unsigned passport_animation_frame(passport_animation_t *p,uint32_t now) {
    if(!p || !p->visible) return 0;
    if(now-p->began>10000u) rest(p,now);
    /* Bounded catch-up, never replay all missed frames after a stalled UI. */
    for(unsigned advance=0;advance<16;advance++) {
        const keyframe_t *clip=idle;
        unsigned count=sizeof idle/sizeof idle[0];
        if(p->state==PASSPORT_ANIMATION_FOCUS_AGENT) {clip=near;count=sizeof near/sizeof near[0];}
        else if(p->state==PASSPORT_ANIMATION_FOCUS_QR) {clip=far;count=sizeof far/sizeof far[0];}
        else if(p->state==PASSPORT_ANIMATION_NOD) {clip=nod;count=sizeof nod/sizeof nod[0];}
        uint32_t duration=(!p->state && !p->step)?p->dwell:clip[p->step].duration;
        if(now-p->began<duration) return clip[p->step].frame;
        p->began+=duration;
        if(++p->step>=count) rest(p,p->began);
    }
    rest(p,now);
    return 0;
}
unsigned passport_animation_loop_frame(const uint16_t *durations,unsigned count,uint32_t began,uint32_t now) {
    if(!durations || !count || count>64) return 0;
    uint32_t total=0;
    for(unsigned i=0;i<count;i++) {if(!durations[i]) return 0;total+=durations[i];}
    uint32_t elapsed=(now-began)%total;
    for(unsigned i=0;i<count;i++) {if(elapsed<durations[i]) return i;elapsed-=durations[i];}
    return 0;
}
unsigned passport_animation_agent_frame(passport_agent_state_t state,passport_voice_state_t voice,uint32_t elapsed) {
    static const uint8_t listening[]={10,11,11,10,0};
    static const uint8_t thinking[]={5,6,7,8,9,0};
    static const uint8_t talking[]={13,14,15,16,0};
    if(voice==PASSPORT_VOICE_PLAYING) return talking[(elapsed/140)%5];
    if(state==PASSPORT_AGENT_RECORDING) return listening[(elapsed/350)%5];
    if(state==PASSPORT_AGENT_WAITING || voice==PASSPORT_VOICE_PREPARING) return thinking[(elapsed/250)%6];
    if(state==PASSPORT_AGENT_REPLY && voice==PASSPORT_VOICE_DONE && elapsed<440) return nod[(elapsed/90)%5].frame;
    uint32_t phase=elapsed%4200;
    return phase>=3900 && phase<4050?3:phase<700?1:0;
}
unsigned passport_animation_nora_frame(passport_agent_state_t state,passport_voice_state_t voice,uint32_t elapsed,uint8_t level) {
    if(voice==PASSPORT_VOICE_PLAYING) {
        static const uint8_t speaking[]={5,6,5,0};
        return level<5?0:speaking[(elapsed/150)%4];
    }
    uint32_t blink=elapsed%4200;
    if(blink>=3500 && blink<3800) return blink<3600 || blink>=3700?1:2;
    if(state==PASSPORT_AGENT_RECORDING) return 3;
    if(state==PASSPORT_AGENT_WAITING || voice==PASSPORT_VOICE_PREPARING) return elapsed%1500<1000?4:0;
    if(state==PASSPORT_AGENT_REPLY && voice==PASSPORT_VOICE_DONE && elapsed<900) return 7;
    return 0;
}
