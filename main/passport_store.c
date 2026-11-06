#include "passport_store.h"
#include "passport_model.h"
#include <string.h>

#define RECORD_HEADER 12
#define RECORD_PAYLOAD (1 + PASSPORT_NAME_MAX + 1 + PASSPORT_TITLE_MAX + 1 + \
                        PASSPORT_RESEARCH_MAX + 1 + PASSPORT_BIO_MAX + 1 + PASSPORT_HOMEPAGE_MAX + 1)
#define RECORD_SIZE (RECORD_HEADER + RECORD_PAYLOAD)

static uint32_t crc32(const uint8_t *data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xedb88320u);
    }
    return ~crc;
}
static void put16(uint8_t *p, uint16_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static void put32(uint8_t *p, uint32_t n) { for(unsigned i=0;i<4;++i) p[i]=(uint8_t)(n>>(8*i)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1]<<8)); }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static bool valid_record(const passport_record_t *r) {
    return r && r->schema == 1 && passport_brightness_valid(r->brightness)
        && passport_profile_validate(&r->profile);
}
static void encode_fields(uint8_t *dst, const passport_profile_t *p) {
    const char *fields[] = {p->name,p->title,p->research,p->bio,p->homepage};
    const size_t sizes[] = {sizeof p->name,sizeof p->title,sizeof p->research,sizeof p->bio,sizeof p->homepage};
    for (unsigned i=0;i<5;++i) { memcpy(dst,fields[i],strlen(fields[i])+1); dst+=sizes[i]; }
}
static void decode_fields(passport_profile_t *p, const uint8_t *src) {
    char *fields[] = {p->name,p->title,p->research,p->bio,p->homepage};
    const size_t sizes[] = {sizeof p->name,sizeof p->title,sizeof p->research,sizeof p->bio,sizeof p->homepage};
    for (unsigned i=0;i<5;++i) { memcpy(fields[i],src,sizes[i]); src+=sizes[i]; }
}

passport_status_t passport_store_load(const passport_store_backend_t *b, passport_record_t *out) {
    if (!b || !b->read || !out) return PASSPORT_ERR_INVALID;
    uint8_t buffer[RECORD_SIZE];
    size_t size = sizeof buffer;
    passport_status_t status = b->read(b->ctx, buffer, &size);
    if (status != PASSPORT_OK) return status == PASSPORT_ERR_CAPACITY ? PASSPORT_ERR_CORRUPT : status;
    if (size != RECORD_SIZE || memcmp(buffer,"PASP",4) || get16(buffer+4)!=1
        || get16(buffer+6)!=RECORD_PAYLOAD || get32(buffer+8)!=crc32(buffer+RECORD_HEADER,RECORD_PAYLOAD))
        return PASSPORT_ERR_CORRUPT;
    passport_record_t candidate = {.schema=1, .brightness=buffer[RECORD_HEADER]};
    decode_fields(&candidate.profile, buffer+RECORD_HEADER+1);
    if (!valid_record(&candidate)) return PASSPORT_ERR_CORRUPT;
    *out = candidate;
    return PASSPORT_OK;
}

passport_status_t passport_store_save(const passport_store_backend_t *b, passport_record_t *current,
                                      const passport_record_t *candidate) {
    if (!b || !b->write || !current || !valid_record(candidate)) return PASSPORT_ERR_INVALID;
    uint8_t buffer[RECORD_SIZE] = {0};
    memcpy(buffer,"PASP",4); put16(buffer+4,1); put16(buffer+6,RECORD_PAYLOAD);
    buffer[RECORD_HEADER] = candidate->brightness;
    encode_fields(buffer+RECORD_HEADER+1,&candidate->profile);
    put32(buffer+8,crc32(buffer+RECORD_HEADER,RECORD_PAYLOAD));
    passport_status_t status = b->write(b->ctx,buffer,sizeof buffer);
    if (status == PASSPORT_OK) *current = *candidate;
    return status;
}
