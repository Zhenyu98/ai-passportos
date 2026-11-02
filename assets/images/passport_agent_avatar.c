/* Public builds use the neutral avatar; personal Muse art stays private. */
#include "passport_assets.h"
const char passport_agent_name[] = "Muse";
const lv_image_dsc_t *passport_agent_avatar_frame(unsigned frame) {
    static const uint8_t neutral[] = {0,1,2,0,0,0,0,0};
    return &passport_avatar_frames[neutral[frame<8?frame:0]];
}
