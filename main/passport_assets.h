#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LV_FONT_DECLARE(passport_font_16);
LV_FONT_DECLARE(passport_font_20);
LV_FONT_DECLARE(passport_font_24_bold);
extern const lv_image_dsc_t passport_avatar_frames[3];
extern const bool passport_avatar_is_temporary;
/* Optional pre-rendered idle/focus/nod set, exactly 18 frames when enabled. */
extern const lv_image_dsc_t passport_motion_frames[];
extern const size_t passport_motion_count;
typedef struct {
    uint8_t id; /* Stable stored identifier: 1 default, 2 night, 3 cat. */
    const char *name;
    const lv_image_dsc_t *frames;
    const uint16_t *durations;
    size_t frame_count;
    bool interactive;
} passport_motion_scene_t;
extern const passport_motion_scene_t passport_motion_scenes[];
extern const size_t passport_motion_scene_count;
const passport_motion_scene_t *passport_scene_by_id(unsigned id);
const passport_motion_scene_t *passport_scene_by_image(const lv_image_dsc_t *first_frame);
/* Embedded pixel-scene gallery; scenes can be selected from the config page. */
extern const lv_image_dsc_t passport_gallery_frames[];
extern const char *const passport_gallery_names[];
extern const size_t passport_gallery_count;
/* Returns the active avatar: an uploaded flash image when present, else frame 0. */
const lv_image_dsc_t *passport_avatar_current(void);
/* Independent Muse character; never changes the selected identity-card avatar. */
extern const char passport_agent_name[];
const lv_image_dsc_t *passport_agent_avatar_frame(unsigned frame);
/* Returns the full-screen picture (uploaded card/QR image), or NULL when absent. */
const lv_image_dsc_t *passport_image_current(void);
extern const uint32_t passport_asset_glyphs[];
extern const size_t passport_asset_glyph_count;
bool passport_assets_has_glyph(uint32_t codepoint,void *user);
