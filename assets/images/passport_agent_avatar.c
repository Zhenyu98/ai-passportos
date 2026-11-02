/* Public builds reuse the showcase character; personal Muse art stays private. */
#include "passport_assets.h"
const char passport_agent_name[] = "Muse";
const lv_image_dsc_t *passport_agent_avatar_frame(unsigned frame) {
    /* Agent poses: neutral, half/closed blink, listen, think, speak a/b, nod. */
    static const uint8_t motion[] = {0,2,3,8,10,13,12,14};
    static const uint8_t neutral[] = {0,1,2,0,0,0,0,0};
    if(frame>=8) frame=0;
    if(passport_motion_count>=18) return &passport_motion_frames[motion[frame]];
    return &passport_avatar_frames[neutral[frame]];
}
