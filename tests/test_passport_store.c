#include "passport_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t data[2048]; size_t size; bool fail; unsigned writes; } memory_store_t;
static passport_status_t read_record(void *ctx, uint8_t *out, size_t *size) {
    memory_store_t *m = ctx;
    if (!m->size) return PASSPORT_ERR_NOT_FOUND;
    if (*size < m->size) return PASSPORT_ERR_CAPACITY;
    memcpy(out, m->data, m->size); *size = m->size;
    return PASSPORT_OK;
}
static passport_status_t write_record(void *ctx, const uint8_t *data, size_t size) {
    memory_store_t *m = ctx; ++m->writes;
    if (m->fail) return PASSPORT_ERR_STORAGE;
    assert(size <= sizeof m->data);
    memcpy(m->data, data, size); m->size = size;
    return PASSPORT_OK;
}
int main(void) {
    memory_store_t memory = {0};
    passport_store_backend_t backend = {.ctx=&memory, .read=read_record, .write=write_record};
    passport_record_t current = {.schema=1, .brightness=60};
    passport_record_t candidate = current, loaded = current;
    assert(passport_store_load(&backend, &loaded) == PASSPORT_ERR_NOT_FOUND);
    strcpy(candidate.profile.name, "Alice");
    strcpy(candidate.profile.homepage, "https://example.org/alice");
    assert(passport_store_save(&backend, &current, &candidate) == PASSPORT_OK);
    assert(strcmp(current.profile.name, "Alice") == 0);
    assert(passport_store_load(&backend, &loaded) == PASSPORT_OK);
    assert(loaded.brightness == 60 && loaded.schema == 1);
    assert(strcmp(loaded.profile.homepage, candidate.profile.homepage) == 0);
    assert(memcmp(memory.data, "PASP", 4) == 0);
    passport_record_t before; memcpy(&before, &current, sizeof before);
    candidate.brightness = 50;
    unsigned writes = memory.writes;
    assert(passport_store_save(&backend, &current, &candidate) == PASSPORT_ERR_INVALID);
    assert(memory.writes == writes && memcmp(&current, &before, sizeof current) == 0);
    candidate.brightness = 90; strcpy(candidate.profile.name, "Bob");
    memory.fail = true;
    assert(passport_store_save(&backend, &current, &candidate) == PASSPORT_ERR_STORAGE);
    assert(memcmp(&current, &before, sizeof current) == 0);
    assert(passport_store_load(&backend, &loaded) == PASSPORT_OK);
    assert(strcmp(loaded.profile.name, "Alice") == 0);
    memory.fail = false;
    memory.data[memory.size - 1] ^= 1;
    assert(passport_store_load(&backend, &loaded) == PASSPORT_ERR_CORRUPT);
    assert(strcmp(loaded.profile.name, "Alice") == 0);
    memory.data[memory.size - 1] ^= 1;
    --memory.size;
    assert(passport_store_load(&backend, &loaded) == PASSPORT_ERR_CORRUPT);
    assert(passport_store_save(&backend, &current, &candidate) == PASSPORT_OK);
    memory.data[4] = 99;
    assert(passport_store_load(&backend, &loaded) == PASSPORT_ERR_CORRUPT);
    puts("Passport store: PASS");
    return 0;
}
