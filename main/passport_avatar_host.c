/* Host preview fallback: no flash partition, always the compiled frame. */
#include "passport_assets.h"

const lv_image_dsc_t *passport_avatar_current(void) {
    const passport_motion_scene_t *scene=passport_scene_by_id(1);
    return scene?scene->frames:&passport_avatar_frames[0];
}

const lv_image_dsc_t *passport_image_current(void) {
    return NULL;
}
