#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Uploaded-media flash storage (firmware only) on the "avatar" partition.
 * Layout: 16-byte header {magic, width, height, format, crc32} followed by
 * pixel data. Two independent slots:
 *  - avatar  @ 0x00000: RGB565A8, only 76x150 (column) or 216x112 (stacked).
 *  - picture @ 0x40000: RGB565, exactly 240x320 (full-screen card image).
 * Uploads stream through begin/chunk/end so no full buffer is needed in RAM. */

#define PASSPORT_AVATAR_MAGIC 0x31564150u /* "PAV1" */
#define PASSPORT_AVATAR_FORMAT_RGB565A8 1
#define PASSPORT_AVATAR_FORMAT_RGB565 2
#define PASSPORT_AVATAR_FORMAT_SCENE 3
#define PASSPORT_PICTURE_WIDTH 240
#define PASSPORT_PICTURE_HEIGHT 320

typedef struct {
    uint32_t magic;
    uint16_t width;
    uint16_t height;
    uint8_t format;
    uint8_t reserved[3];
    uint32_t crc32;
} passport_avatar_header_t;

bool passport_avatar_flash_begin(int width, int height);
bool passport_avatar_flash_chunk(const uint8_t *data, size_t length);
bool passport_avatar_flash_end(void);
/* Cancels a failed upload and releases the stream slot. */
void passport_avatar_flash_abort(void);
/* Drops the stored avatar so the compiled default is used again. */
bool passport_avatar_flash_reset(void);
/* Persist a validated built-in scene ID in the avatar slot, never the picture slot. */
bool passport_avatar_flash_select_scene(unsigned id);
/* Drops the cached in-RAM copy; the next passport_avatar_current() re-reads flash. */
void passport_avatar_flash_invalidate(void);

/* Full-screen picture slot; passport_image_current() returns NULL when absent. */
bool passport_image_flash_begin(int width, int height);
bool passport_image_flash_chunk(const uint8_t *data, size_t length);
bool passport_image_flash_end(void);
void passport_image_flash_abort(void);
bool passport_image_flash_reset(void);
void passport_image_flash_invalidate(void);
const lv_image_dsc_t *passport_image_current(void);
