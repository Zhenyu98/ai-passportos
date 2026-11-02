#pragma once
#include <stdint.h>
typedef struct {uint32_t magic,cf,w,h,stride;} lv_image_header_t;
typedef struct {lv_image_header_t header;uint32_t data_size;const uint8_t *data;} lv_image_dsc_t;
typedef struct {int unused;} lv_font_t;
#define LV_FONT_DECLARE(name) extern const lv_font_t name
#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_RGB565 1
#define LV_COLOR_FORMAT_RGB565A8 2
