#include "passport_assets.h"

const passport_motion_scene_t *passport_scene_by_id(unsigned id) {
    for(size_t i=0;i<passport_motion_scene_count;i++)
        if(passport_motion_scenes[i].id==id) return &passport_motion_scenes[i];
    return NULL;
}
const passport_motion_scene_t *passport_scene_by_image(const lv_image_dsc_t *image) {
    for(size_t i=0;i<passport_motion_scene_count;i++)
        if(passport_motion_scenes[i].frames==image) return &passport_motion_scenes[i];
    return NULL;
}
