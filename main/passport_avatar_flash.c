/* Uploaded-media flash storage on the "avatar" partition.
 * Two slots: avatar (RGB565A8, RAM-cached) and picture (RGB565 240x320,
 * memory-mapped for zero-RAM full-screen rendering).
 * Uploads stream in chunks so the HTTP layer never needs the full payload in
 * RAM. Only the documented layouts are accepted. Cached pixels are released
 * lazily, on the next load, so LVGL never reads memory that was freed or
 * unmapped while it still referenced an image. */
#include "passport_avatar_flash.h"
#include "passport_assets.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>

#define MEDIA_PARTITION_LABEL "avatar"
#define AVATAR_SLOT_OFFSET 0
#define PICTURE_SLOT_OFFSET 0x40000
#define AVATAR_LAYOUT_COUNT 2
static const struct { int width, height; } avatar_layouts[AVATAR_LAYOUT_COUNT] = {
    {76, 150}, {216, 112}
};

static const esp_partition_t *partition(void) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                    MEDIA_PARTITION_LABEL);
}

/* Incremental CRC-32 (reflected, poly 0xEDB88320); caller applies init/final xor.
 * Writers start at 0xffffffff and store the inverted result, matching zlib. */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc;
}

static SemaphoreHandle_t guard;

/* Shared upload stream state; guarded by the same mutex. */
typedef struct {
    bool active;
    uint8_t format;
    int width, height;
    size_t expected, written, offset;
    uint32_t crc;
    uint8_t stage[64] __attribute__((aligned(4)));
    size_t stage_used;
} stream_t;
static stream_t avatar_stream, picture_stream;

/* Fails closed: without the mutex, storage access is refused instead of
 * proceeding unserialized. */
static bool lock(void) {
    if (!guard) guard = xSemaphoreCreateMutex();
    if (!guard) return false;
    (void)xSemaphoreTake(guard, portMAX_DELAY);
    return true;
}
static void unlock(void) { if (guard) xSemaphoreGive(guard); }

/* ---- Avatar slot: flash-mapped RGB565A8 (zero RAM, like the picture) ----
 * Caching the avatar in heap (up to 72 KB) collided with the Wi-Fi hotspot's
 * memory needs and aborted the config task, so the pixel data is memory-mapped
 * from flash and read through the DROM cache instead. */

static lv_image_dsc_t cached_avatar;
static const void *avatar_mapped;
static esp_partition_mmap_handle_t avatar_map;
static bool avatar_cache_valid;
static bool avatar_stale;
static const passport_motion_scene_t *cached_scene;

/* Releases the previous mapping. Runs only from the load path, after the UI has
 * already replaced the image object that referenced it. */
static void avatar_drop_locked(void) {
    if (avatar_mapped) {
        esp_partition_munmap(avatar_map);
        avatar_mapped = NULL;
    }
    avatar_cache_valid = false;
    cached_scene=NULL;
    memset(&cached_avatar,0,sizeof cached_avatar);
}

static bool avatar_load_locked(void) {
    if (avatar_cache_valid) return avatar_mapped != NULL || cached_scene!=NULL;
    avatar_cache_valid = true; /* Latch the outcome of this attempt. */
    const esp_partition_t *part = partition();
    if (!part) return false;
    passport_avatar_header_t header;
    if (esp_partition_read(part, AVATAR_SLOT_OFFSET, &header, sizeof header) != ESP_OK)
        return false;
    if(header.magic!=PASSPORT_AVATAR_MAGIC) return false;
    if(header.format==PASSPORT_AVATAR_FORMAT_SCENE) {
        const passport_motion_scene_t *scene=passport_scene_by_id(header.reserved[0]);
        if(!scene || !scene->frames || !scene->frame_count || header.reserved[1]!=1 || header.reserved[2] ||
           header.width!=scene->frames[0].header.w || header.height!=scene->frames[0].header.h ||
           header.crc32!=~crc32_update(0xffffffffu,(const uint8_t *)&header,offsetof(passport_avatar_header_t,crc32))) return false;
        cached_scene=scene;
        return true;
    }
    if(header.format!=PASSPORT_AVATAR_FORMAT_RGB565A8) return false;
    bool layout = false;
    for (unsigned i = 0; i < AVATAR_LAYOUT_COUNT; ++i)
        layout = layout || (header.width == avatar_layouts[i].width &&
                            header.height == avatar_layouts[i].height);
    if (!layout) return false;
    size_t payload = (size_t)header.width * header.height * 3;
    if (payload > PICTURE_SLOT_OFFSET - sizeof header) return false;
    const void *mapped = NULL;
    esp_partition_mmap_handle_t map;
    if (esp_partition_mmap(part, AVATAR_SLOT_OFFSET + sizeof header, payload,
                           ESP_PARTITION_MMAP_DATA, &mapped, &map) != ESP_OK) {
        ESP_LOGW("passport", "avatar slot mmap failed");
        return false;
    }
    if (~crc32_update(0xffffffffu, mapped, payload) != header.crc32) {
        esp_partition_munmap(map);
        ESP_LOGW("passport", "avatar slot rejected (crc)");
        return false;
    }
    avatar_mapped = mapped;
    avatar_map = map;
    cached_avatar = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC,
                                                .cf = LV_COLOR_FORMAT_RGB565A8,
                                                .w = header.width, .h = header.height,
                                                .stride = (uint32_t)header.width * 2},
                                     .data_size = (uint32_t)payload, .data = mapped};
    return true;
}

const lv_image_dsc_t *passport_avatar_current(void) {
    const passport_motion_scene_t *default_scene=passport_scene_by_id(1);
    const lv_image_dsc_t *result=default_scene?default_scene->frames:&passport_avatar_frames[0];
    if (!lock()) return result;
    if (avatar_stale) {
        avatar_stale = false;
        avatar_drop_locked();
    }
    if(avatar_load_locked()) {
        if(cached_scene) result=cached_scene->frames;
        else if(cached_avatar.data) result=&cached_avatar;
    }
    unlock();
    return result;
}

/* Only marks the cache stale: the pixel memory may still be referenced by the
 * image object LVGL is showing, so it is released on the next load instead. */
void passport_avatar_flash_invalidate(void) {
    if (!lock()) return;
    avatar_stale = true;
    avatar_stream.active = false;
    avatar_stream.stage_used = 0;
    unlock();
}

/* ---- Picture slot: mmap-cached RGB565 240x320 ---- */

static lv_image_dsc_t cached_picture;
static const void *picture_mapped;
static esp_partition_mmap_handle_t picture_map;
static bool picture_cache_valid;
static bool picture_stale;

void passport_image_flash_invalidate(void) {
    if (!lock()) return;
    picture_stale = true;
    picture_stream.active = false;
    picture_stream.stage_used = 0;
    unlock();
}

const lv_image_dsc_t *passport_image_current(void) {
    if (!lock()) return NULL;
    if (picture_stale) {
        picture_stale = false;
        if (picture_mapped) {
            esp_partition_munmap(picture_map);
            picture_mapped = NULL;
        }
        picture_cache_valid = false;
    }
    if (!picture_cache_valid) {
        picture_cache_valid = true; /* Latch the outcome of this attempt. */
        const esp_partition_t *part = partition();
        passport_avatar_header_t header;
        if (part && esp_partition_read(part, PICTURE_SLOT_OFFSET, &header, sizeof header) == ESP_OK &&
            header.magic == PASSPORT_AVATAR_MAGIC &&
            header.format == PASSPORT_AVATAR_FORMAT_RGB565 &&
            header.width == PASSPORT_PICTURE_WIDTH && header.height == PASSPORT_PICTURE_HEIGHT) {
            size_t payload = (size_t)header.width * header.height * 2;
            const void *mapped = NULL;
            esp_partition_mmap_handle_t map;
            if (esp_partition_mmap(part, PICTURE_SLOT_OFFSET + sizeof header, payload,
                                   ESP_PARTITION_MMAP_DATA, &mapped, &map) == ESP_OK &&
                ~crc32_update(0xffffffffu, mapped, payload) == header.crc32) {
                picture_mapped = mapped;
                picture_map = map;
                cached_picture = (lv_image_dsc_t){
                    .header = {.magic = LV_IMAGE_HEADER_MAGIC,
                               .cf = LV_COLOR_FORMAT_RGB565,
                               .w = PASSPORT_PICTURE_WIDTH, .h = PASSPORT_PICTURE_HEIGHT,
                               .stride = (uint32_t)PASSPORT_PICTURE_WIDTH * 2},
                    .data_size = (uint32_t)payload, .data = (const uint8_t *)mapped};
            } else {
                if (mapped) esp_partition_munmap(map);
                ESP_LOGW("passport", "picture slot rejected (mmap/crc)");
            }
        }
    }
    const lv_image_dsc_t *result = picture_mapped ? &cached_picture : NULL;
    unlock();
    return result;
}

/* ---- Shared streaming write path ---- */

static bool stream_begin(stream_t *stream, size_t offset, uint8_t format,
                         int width, int height) {
    if (width <= 0 || height <= 0) return false;
    size_t bytes_per = format == PASSPORT_AVATAR_FORMAT_RGB565A8 ? 3 : 2;
    size_t payload = (size_t)width * height * bytes_per;
    const esp_partition_t *part = partition();
    if (!part) return false;
    size_t span = ((sizeof(passport_avatar_header_t) + payload + 4095u) / 4096u) * 4096u;
    if (offset + span > (size_t)part->size) return false;
    if (!lock()) return false;
    bool ok = !stream->active;
    if (ok) {
        if (esp_partition_erase_range(part, offset, span) != ESP_OK) ok = false;
        else {
            stream->active = true;
            stream->format = format;
            stream->width = width; stream->height = height;
            stream->expected = payload; stream->written = 0; stream->offset = offset;
            stream->crc = 0xffffffffu; stream->stage_used = 0;
        }
    }
    unlock();
    return ok;
}

/* Writes need 4-byte alignment; accepted layouts keep every full chunk aligned,
 * so only the final pad to the sector boundary may need zero padding. */
static bool stream_flush(stream_t *stream, size_t pad_limit) {
    while (stream->stage_used >= 4 || (stream->stage_used && pad_limit)) {
        size_t flush = stream->stage_used & ~(size_t)3u;
        if (!flush) flush = 4;
        if (flush > stream->stage_used) {
            memset(stream->stage + stream->stage_used, 0, flush - stream->stage_used);
            stream->stage_used = flush;
        }
        if (esp_partition_write(partition(), stream->offset + sizeof(passport_avatar_header_t) +
                                stream->written, stream->stage, flush) != ESP_OK)
            return false;
        stream->written += flush;
        memmove(stream->stage, stream->stage + flush, stream->stage_used - flush);
        stream->stage_used -= flush;
        if (stream->stage_used == 0) break;
    }
    return true;
}

static bool stream_chunk(stream_t *stream, const uint8_t *data, size_t length) {
    if (!data || !length) return false;
    if (!lock()) return false;
    bool ok = stream->active && stream->written + stream->stage_used + length <= stream->expected;
    while (ok && length) {
        stream->crc = crc32_update(stream->crc, data, 1); /* CRC over the exact stream */
        stream->stage[stream->stage_used++] = *data++;
        --length;
        if (stream->stage_used == sizeof stream->stage) ok = stream_flush(stream, 0);
    }
    unlock();
    return ok;
}

static bool stream_end(stream_t *stream) {
    if (!lock()) return false;
    bool ok = stream->active && stream->written + stream->stage_used == stream->expected;
    if (ok) ok = stream_flush(stream, 1); /* Pad the tail so the sector write is aligned. */
    if (ok) {
        passport_avatar_header_t header = {.magic = PASSPORT_AVATAR_MAGIC,
                                           .width = (uint16_t)stream->width,
                                           .height = (uint16_t)stream->height,
                                           .format = stream->format,
                                           .crc32 = ~stream->crc};
        ok = esp_partition_write(partition(), stream->offset, &header, sizeof header) == ESP_OK;
    }
    stream->active = false;
    stream->stage_used = 0;
    unlock();
    return ok;
}

static void stream_abort(stream_t *stream) {
    if (!lock()) return;
    stream->active = false;
    stream->stage_used = 0;
    unlock();
}

static bool stream_reset(size_t offset) {
    const esp_partition_t *part = partition();
    if (!part) return false;
    if (!lock()) return false;
    bool ok = esp_partition_erase_range(part, offset, 4096) == ESP_OK;
    unlock();
    return ok;
}

/* ---- Avatar-slot public API ---- */

bool passport_avatar_flash_select_scene(unsigned id) {
    const passport_motion_scene_t *scene=passport_scene_by_id(id);
    const esp_partition_t *part=partition();
    if(!scene || !scene->frames || !scene->frame_count || id>255 || !part || part->size<4096 || !lock()) return false;
    bool ok=false;
    if(!avatar_stream.active) {
        passport_avatar_header_t header={.magic=PASSPORT_AVATAR_MAGIC,
            .width=scene->frames[0].header.w,.height=scene->frames[0].header.h,
            .format=PASSPORT_AVATAR_FORMAT_SCENE,.reserved={(uint8_t)id,1,0}};
        header.crc32=~crc32_update(0xffffffffu,(const uint8_t *)&header,offsetof(passport_avatar_header_t,crc32));
        ok=esp_partition_erase_range(part,AVATAR_SLOT_OFFSET,4096)==ESP_OK &&
           esp_partition_write(part,AVATAR_SLOT_OFFSET,&header,sizeof header)==ESP_OK;
        passport_avatar_header_t readback;
        if(ok) ok=esp_partition_read(part,AVATAR_SLOT_OFFSET,&readback,sizeof readback)==ESP_OK &&
                  !memcmp(&header,&readback,sizeof header);
        avatar_stale=true;
    }
    unlock();return ok;
}

bool passport_avatar_flash_begin(int width, int height) {
    bool layout = false;
    for (unsigned i = 0; i < AVATAR_LAYOUT_COUNT; ++i)
        layout = layout || (width == avatar_layouts[i].width && height == avatar_layouts[i].height);
    if (!layout) return false;
    return stream_begin(&avatar_stream, AVATAR_SLOT_OFFSET, PASSPORT_AVATAR_FORMAT_RGB565A8,
                        width, height);
}

bool passport_avatar_flash_chunk(const uint8_t *data, size_t length) {
    return stream_chunk(&avatar_stream, data, length);
}

bool passport_avatar_flash_end(void) {
    bool ok = stream_end(&avatar_stream);
    if (ok) {
        passport_avatar_flash_invalidate();
        ESP_LOGI("passport", "avatar stored %dx%d", avatar_stream.width, avatar_stream.height);
    }
    return ok;
}

void passport_avatar_flash_abort(void) { stream_abort(&avatar_stream); }

bool passport_avatar_flash_reset(void) {
    if (!stream_reset(AVATAR_SLOT_OFFSET)) return false;
    passport_avatar_flash_invalidate();
    return true;
}

/* ---- Picture-slot public API ---- */

bool passport_image_flash_begin(int width, int height) {
    if (width != PASSPORT_PICTURE_WIDTH || height != PASSPORT_PICTURE_HEIGHT) return false;
    return stream_begin(&picture_stream, PICTURE_SLOT_OFFSET, PASSPORT_AVATAR_FORMAT_RGB565,
                        width, height);
}

bool passport_image_flash_chunk(const uint8_t *data, size_t length) {
    return stream_chunk(&picture_stream, data, length);
}

bool passport_image_flash_end(void) {
    bool ok = stream_end(&picture_stream);
    if (ok) {
        passport_image_flash_invalidate();
        ESP_LOGI("passport", "picture stored %dx%d", picture_stream.width, picture_stream.height);
    }
    return ok;
}

void passport_image_flash_abort(void) { stream_abort(&picture_stream); }

bool passport_image_flash_reset(void) {
    if (!stream_reset(PICTURE_SLOT_OFFSET)) return false;
    passport_image_flash_invalidate();
    return true;
}
