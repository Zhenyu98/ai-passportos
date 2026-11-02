#include "passport_nvs.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t data[2048];
static size_t data_size;
static unsigned writes, erases, fail_commit;
static bool fail_open, fail_set;
static unsigned fail_set_after_write;

esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(strcmp(name,"passport")==0 && mode==NVS_READWRITE);
    if (fail_open) return ESP_FAIL;
    *handle=1; return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { assert(handle==1); }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
    assert(h==1 && strcmp(key,"record")==0);
    if (!data_size) return ESP_ERR_NVS_NOT_FOUND;
    if (!out) { *size=data_size; return ESP_OK; }
    if (*size<data_size) { *size=data_size; return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(out,data,data_size); *size=data_size; return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *in, size_t size) {
    assert(h==1 && strcmp(key,"record")==0 && size<=sizeof data);
    ++writes;
    if (fail_set) return ESP_FAIL;
    /* Like the current NVS implementation, set may write before commit. */
    memcpy(data,in,size); data_size=size;
    if(fail_set_after_write) {--fail_set_after_write;return ESP_ERR_NVS_REMOVE_FAILED;}
    return ESP_OK;
}
esp_err_t nvs_erase_key(nvs_handle_t h, const char *key) {
    assert(h==1 && strcmp(key,"record")==0);
    ++erases; data_size=0; return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
    assert(h==1);
    if (fail_commit) { --fail_commit; return ESP_FAIL; }
    return ESP_OK;
}
int main(void) {
    passport_store_backend_t backend;
    fail_open=true;
    assert(passport_nvs_open(&backend)==ESP_FAIL);
    fail_open=false;
    assert(passport_nvs_open(&backend)==ESP_OK);
    passport_record_t current={.schema=1,.brightness=60}, candidate=current, loaded=current;
    strcpy(candidate.profile.name,"Alice");
    assert(passport_store_save(&backend,&current,&candidate)==PASSPORT_OK);
    strcpy(candidate.profile.name,"Bob");
    fail_commit=1;
    assert(passport_store_save(&backend,&current,&candidate)==PASSPORT_ERR_STORAGE);
    assert(strcmp(current.profile.name,"Alice")==0);
    assert(passport_store_load(&backend,&loaded)==PASSPORT_OK);
    assert(strcmp(loaded.profile.name,"Alice")==0);
    fail_set_after_write=1;
    assert(passport_store_save(&backend,&current,&candidate)==PASSPORT_ERR_STORAGE);
    assert(passport_store_load(&backend,&loaded)==PASSPORT_OK);
    assert(strcmp(loaded.profile.name,"Alice")==0);
    fail_set=true;
    assert(passport_store_save(&backend,&current,&candidate)==PASSPORT_ERR_STORAGE);
    assert(erases==0);
    fail_set=false;
    passport_nvs_close(); data_size=0;
    assert(passport_nvs_open(&backend)==ESP_OK);
    fail_commit=1;
    assert(passport_store_save(&backend,&current,&candidate)==PASSPORT_ERR_STORAGE);
    assert(data_size==0 && erases==1);
    passport_nvs_close();
    assert(backend.read(backend.ctx,data,&data_size)==PASSPORT_ERR_STORAGE);
    assert(writes>=3);
    puts("Passport NVS failure handling: PASS");
    return 0;
}
