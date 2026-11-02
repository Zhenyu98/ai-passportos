#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "passport_agent_model.h"

#define PASSPORT_ANIMATION_FRAME_COUNT 18u
typedef enum {
    PASSPORT_ANIMATION_FOCUS_AGENT = 1,
    PASSPORT_ANIMATION_FOCUS_QR,
    PASSPORT_ANIMATION_NOD
} passport_animation_event_t;

/* No LVGL/platform pointers or allocation. Times use wrapping millisecond ticks. */
typedef struct {
    uint32_t began, random;
    uint16_t dwell;
    uint8_t state, step;
    bool visible;
} passport_animation_t;

void passport_animation_show(passport_animation_t *, uint32_t now_ms);
void passport_animation_hide(passport_animation_t *);
void passport_animation_event(passport_animation_t *, passport_animation_event_t, uint32_t now_ms);
unsigned passport_animation_frame(passport_animation_t *, uint32_t now_ms);
unsigned passport_animation_loop_frame(const uint16_t *durations,unsigned count,
                                       uint32_t began_ms,uint32_t now_ms);
unsigned passport_animation_agent_frame(passport_agent_state_t,passport_voice_state_t,uint32_t elapsed_ms);
unsigned passport_animation_nora_frame(passport_agent_state_t,passport_voice_state_t,uint32_t elapsed_ms,uint8_t level);
