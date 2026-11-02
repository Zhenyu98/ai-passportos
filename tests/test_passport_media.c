/* Host test for the media slots: exact zlib-compatible CRCs, unaligned chunk
 * streaming, readback, LVGL-safe cache lifetime and fail-closed behaviour. */
#include "../main/passport_avatar_flash.c"
#include <assert.h>
#include <stdio.h>
static uint8_t flash[0x80000],payload[240*320*2];
static esp_partition_t part={sizeof flash};
static unsigned mappings,erase_calls,write_calls;
static bool fail_mutex,fail_write,corrupt_write;
const lv_image_dsc_t passport_avatar_frames[3]={{{.w=64,.h=64},0,NULL}};
const lv_image_dsc_t passport_motion_frames[1]={{{.w=76,.h=150},0,NULL}};
const size_t passport_motion_count=18;
static const lv_image_dsc_t night_frames[6]={{{.w=216,.h=112},0,NULL}};
static const lv_image_dsc_t cat_frames[6]={{{.w=76,.h=150},0,NULL}};
static const uint16_t scene_durations[6]={800,180,220,160,180,770};
const passport_motion_scene_t passport_motion_scenes[]={
    {1,"Default",passport_motion_frames,NULL,18,true},
    {2,"Night",night_frames,scene_durations,6,false},
    {3,"Cat",cat_frames,scene_durations,6,false}};
const size_t passport_motion_scene_count=3;
const esp_partition_t *esp_partition_find_first(int type,int subtype,const char *label) {
    assert(type==1 && subtype==-1 && !strcmp(label,"avatar"));return &part;
}
esp_err_t esp_partition_read(const esp_partition_t *p,size_t offset,void *data,size_t size) {
    assert(p==&part && offset+size<=sizeof flash);memcpy(data,flash+offset,size);return ESP_OK;
}
esp_err_t esp_partition_write(const esp_partition_t *p,size_t offset,const void *data,size_t size) {
    assert(p==&part && offset+size<=sizeof flash && !(offset%4) && !(size%4));
    ++write_calls;if(fail_write)return ESP_FAIL;
    memcpy(flash+offset,data,size);
    if(corrupt_write && offset>=PICTURE_SLOT_OFFSET+sizeof(passport_avatar_header_t)) {
        flash[offset]^=1;corrupt_write=false;
    }
    return ESP_OK;
}
esp_err_t esp_partition_erase_range(const esp_partition_t *p,size_t offset,size_t size) {
    assert(p==&part && offset+size<=sizeof flash && !(offset%4096) && !(size%4096));
    ++erase_calls;memset(flash+offset,0xff,size);return ESP_OK;
}
esp_err_t esp_partition_mmap(const esp_partition_t *p,size_t offset,size_t size,int kind,const void **data,esp_partition_mmap_handle_t *handle) {
    (void)kind;assert(p==&part && offset+size<=sizeof flash);
    *data=flash+offset;*handle=1;++mappings;return ESP_OK;
}
void esp_partition_munmap(esp_partition_mmap_handle_t handle) {assert(handle==1 && mappings);--mappings;}
void *heap_caps_malloc(size_t size,unsigned caps) {(void)caps;return malloc(size);}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {static int token;return fail_mutex?NULL:&token;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t ticks) {(void)ticks;assert(s);return pdTRUE;}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {assert(s);return pdTRUE;}
static void upload_picture(void) {
    assert(passport_image_flash_begin(240,320));
    for(size_t at=0;at<sizeof payload;) {
        size_t n=sizeof payload-at;if(n>997)n=997; /* Deliberately unaligned chunks. */
        assert(passport_image_flash_chunk(payload+at,n));at+=n;
    }
    assert(passport_image_flash_end());
}
int main(void) {
    memset(flash,0xff,sizeof flash);
    assert(passport_avatar_current()==&passport_motion_frames[0]);
    for(size_t i=0;i<sizeof payload;i++)payload[i]=(uint8_t)(i*37+11);
    upload_picture();
    assert(write_calls>0); /* Payload plus header went through esp_partition_write. */
    passport_avatar_header_t h;memcpy(&h,flash+PICTURE_SLOT_OFFSET,sizeof h);
    assert(h.crc32==0xef803f0du); /* Independently computed zlib CRC32. */
    const lv_image_dsc_t *picture=passport_image_current();
    assert(picture && picture->header.w==240 && picture->data_size==sizeof payload);
    assert(!memcmp(picture->data,payload,sizeof payload));
    /* Re-uploading must not unmap the mapping LVGL is currently displaying. */
    unsigned live_maps=mappings;assert(live_maps==1);
    upload_picture();
    assert(mappings==live_maps);
    /* Invalidation only marks the cache stale; the next load releases the old
     * mapping, so a rendering frame can never read unmapped memory. */
    passport_image_flash_invalidate();
    assert(mappings==live_maps);
    assert(passport_image_current() && mappings==1);
    /* A corrupted payload keeps a well-formed header, so the CRC must reject it. */
    passport_image_flash_invalidate();
    corrupt_write=true;upload_picture();
    assert(!passport_image_current());
    passport_image_flash_invalidate();
    assert(passport_avatar_flash_begin(76,150));
    assert(passport_avatar_flash_chunk(payload,76*150*3));
    assert(passport_avatar_flash_end());
    memcpy(&h,flash,sizeof h);assert(h.crc32==0x83dbd16du);
    const lv_image_dsc_t *avatar=passport_avatar_current();
    assert(avatar!=&passport_avatar_frames[0] && avatar->header.w==76);
    assert(!memcmp(avatar->data,payload,76*150*3));
    passport_avatar_flash_invalidate();
    fail_write=true;
    assert(passport_image_flash_begin(240,320));
    assert(!passport_image_flash_chunk(payload,128));
    passport_image_flash_abort();fail_write=false;
    upload_picture();
    passport_image_flash_invalidate();assert(passport_image_current());
    passport_image_flash_invalidate();assert(mappings==2); /* Avatar + picture mappings are both live. */
    assert(passport_avatar_flash_reset());
    assert(mappings==2); /* Reset only invalidates; the old LVGL reference remains mapped. */
    assert(passport_avatar_current()==&passport_motion_frames[0]);
    assert(mappings==1 && passport_image_current()); /* Default selection preserves the picture slot. */
    unsigned before_selection=erase_calls;
    assert(!passport_avatar_flash_select_scene(99) && erase_calls==before_selection);
    avatar_stream.active=true;
    assert(!passport_avatar_flash_select_scene(2) && erase_calls==before_selection);
    avatar_stream.active=false;
    assert(passport_avatar_flash_select_scene(2));
    assert(passport_avatar_current()==&night_frames[0]);
    passport_avatar_flash_invalidate(); /* Simulate cache recreation after reboot. */
    assert(passport_avatar_current()==&night_frames[0]);
    assert(passport_image_current() && !memcmp(picture->data,payload,sizeof payload));
    assert(passport_avatar_flash_select_scene(3));
    assert(passport_avatar_current()==&cat_frames[0]);
    flash[offsetof(passport_avatar_header_t,reserved)]^=1;
    passport_avatar_flash_invalidate();
    assert(passport_avatar_current()==&passport_motion_frames[0]);
    assert(passport_avatar_flash_select_scene(1));
    assert(passport_avatar_current()==&passport_motion_frames[0]);
    /* Without the mutex every write path fails closed and leaves flash alone. */
    guard=NULL;fail_mutex=true;unsigned erased=erase_calls;
    assert(!passport_image_flash_begin(240,320) && erased==erase_calls);
    assert(!passport_image_flash_reset() && erased==erase_calls);
    assert(!passport_avatar_flash_reset() && erased==erase_calls);
    puts("Media CRC, aligned stream, readback, lifetime and fail-closed: PASS");
    return 0;
}
