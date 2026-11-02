#include "passport_animation.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
    assert(passport_animation_nora_frame(PASSPORT_AGENT_REPLY,PASSPORT_VOICE_PLAYING,0,0)==0);
    assert(passport_animation_nora_frame(PASSPORT_AGENT_REPLY,PASSPORT_VOICE_PLAYING,0,70)==5);
    assert(passport_animation_nora_frame(PASSPORT_AGENT_REPLY,PASSPORT_VOICE_PLAYING,160,70)==6);
    assert(passport_animation_nora_frame(PASSPORT_AGENT_RECORDING,PASSPORT_VOICE_READY,0,50)==3);
    assert(passport_animation_nora_frame(PASSPORT_AGENT_WAITING,PASSPORT_VOICE_READY,0,0)==4);
    for(unsigned state=0;state<=PASSPORT_AGENT_ERROR;state++) for(uint32_t now=0;now<9000;now+=31)
        assert(passport_animation_nora_frame((passport_agent_state_t)state,PASSPORT_VOICE_READY,now,30)<8);
    assert(passport_animation_agent_frame(PASSPORT_AGENT_RECORDING,PASSPORT_VOICE_READY,0)!=
           passport_animation_agent_frame(PASSPORT_AGENT_RECORDING,PASSPORT_VOICE_READY,700));
    assert(passport_animation_agent_frame(PASSPORT_AGENT_WAITING,PASSPORT_VOICE_READY,0)!=
           passport_animation_agent_frame(PASSPORT_AGENT_WAITING,PASSPORT_VOICE_READY,700));
    assert(passport_animation_agent_frame(PASSPORT_AGENT_REPLY,PASSPORT_VOICE_PLAYING,0)!=
           passport_animation_agent_frame(PASSPORT_AGENT_REPLY,PASSPORT_VOICE_PLAYING,200));
    for(unsigned state=0;state<=PASSPORT_AGENT_ERROR;state++) for(uint32_t tick=0;tick<5000;tick+=37)
        assert(passport_animation_agent_frame((passport_agent_state_t)state,PASSPORT_VOICE_READY,tick)<18);
    passport_animation_t player;
    assert(sizeof player <= 32);
    passport_animation_show(&player, 0);
    assert(passport_animation_frame(&player, 0) == 0);
    bool breathed = false, blinked = false;
    for (uint32_t now = 0; now < 8000; now += 10) {
        unsigned frame = passport_animation_frame(&player, now);
        assert(frame < PASSPORT_ANIMATION_FRAME_COUNT);
        breathed |= frame == 1;
        blinked |= frame == 3;
    }
    assert(breathed && blinked);

    passport_animation_show(&player, 0);
    passport_animation_event(&player, PASSPORT_ANIMATION_NOD, 100);
    assert(passport_animation_frame(&player, 100) == 13);
    assert(passport_animation_frame(&player, 179) == 13);
    assert(passport_animation_frame(&player, 180) == 14);
    assert(passport_animation_frame(&player, 280) == 15);
    assert(passport_animation_frame(&player, 360) == 16);
    assert(passport_animation_frame(&player, 460) == 0);
    assert(passport_animation_frame(&player, 540) == 0);

    passport_animation_event(&player, PASSPORT_ANIMATION_FOCUS_QR, 600);
    assert(passport_animation_frame(&player, 600) == 10);
    assert(passport_animation_frame(&player, 700) == 11);
    /* A new target replaces the old gesture, including a nod in progress. */
    passport_animation_event(&player, PASSPORT_ANIMATION_FOCUS_AGENT, 750);
    assert(passport_animation_frame(&player, 750) == 13);
    assert(passport_animation_frame(&player, 900) == 0);
    assert(passport_animation_frame(&player, 1100) == 0);
    passport_animation_event(&player, PASSPORT_ANIMATION_NOD, 1200);
    passport_animation_event(&player, PASSPORT_ANIMATION_FOCUS_QR, 1220);
    assert(passport_animation_frame(&player, 1220) == 10);
    assert(passport_animation_frame(&player, 2000) == 0);

    passport_animation_hide(&player);
    const uint16_t loop[]={800,180,220,160,180,770};
    assert(passport_animation_loop_frame(loop,6,100,100)==0);
    assert(passport_animation_loop_frame(loop,6,100,900)==1);
    assert(passport_animation_loop_frame(loop,6,100,2410)==0);
    assert(passport_animation_loop_frame(loop,6,UINT32_MAX-40u,59)==0);
    const uint16_t invalid[]={0,100};
    assert(passport_animation_loop_frame(invalid,2,0,500)==0);
    passport_animation_event(&player, PASSPORT_ANIMATION_NOD, 2100);
    assert(passport_animation_frame(&player, 2200) == 0);
    passport_animation_show(&player, UINT32_MAX - 40u);
    passport_animation_event(&player, PASSPORT_ANIMATION_NOD, UINT32_MAX - 40u);
    assert(passport_animation_frame(&player, 39) == 14);
    /* A long stall rebases to neutral in bounded work; no stale replay. */
    assert(passport_animation_frame(&player, 3600000) == 0);
    passport_animation_event(&player, (passport_animation_event_t)99, 3600010);
    assert(passport_animation_frame(&player, 3600010) == 0);
    passport_animation_hide(&player);
    puts("Passport animation timing, preemption, wrap and hide: PASS");
    return 0;
}
