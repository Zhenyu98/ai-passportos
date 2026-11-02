#include "passport_nvs.h"
#include "nvs.h"
#include <stdlib.h>

static nvs_handle_t s_handle;
static bool s_open;

static passport_status_t read_blob(void *ctx, uint8_t *buffer, size_t *length) {
    (void)ctx;
    if (!s_open) return PASSPORT_ERR_STORAGE;
    esp_err_t e = nvs_get_blob(s_handle, "record", buffer, length);
    if (e == ESP_ERR_NVS_NOT_FOUND) return PASSPORT_ERR_NOT_FOUND;
    if (e == ESP_ERR_NVS_INVALID_LENGTH) return PASSPORT_ERR_CAPACITY;
    return e == ESP_OK ? PASSPORT_OK : PASSPORT_ERR_STORAGE;
}
static passport_status_t write_blob(void *ctx, const uint8_t *buffer, size_t length) {
    (void)ctx;
    if (!s_open) return PASSPORT_ERR_STORAGE;
    size_t previous_length = 0;
    esp_err_t previous_status = nvs_get_blob(s_handle, "record", NULL, &previous_length);
    bool had_previous = previous_status == ESP_OK;
    if (!had_previous && previous_status != ESP_ERR_NVS_NOT_FOUND) return PASSPORT_ERR_STORAGE;
    if (previous_length > 2048) return PASSPORT_ERR_STORAGE;
    uint8_t *previous = had_previous ? malloc(previous_length ? previous_length : 1) : NULL;
    if (had_previous && (!previous || nvs_get_blob(s_handle, "record", previous, &previous_length) != ESP_OK)) {
        free(previous);
        return PASSPORT_ERR_STORAGE;
    }
    esp_err_t e = nvs_set_blob(s_handle, "record", buffer, length);
    if (e == ESP_OK) e = nvs_commit(s_handle);
    if (e == ESP_OK) { free(previous); return PASSPORT_OK; }
    /* Both set_blob and commit may fail after the new value reaches flash.
     * ESP_ERR_NVS_REMOVE_FAILED specifically permits this durable mutation. */
    esp_err_t rollback = had_previous
        ? nvs_set_blob(s_handle, "record", previous, previous_length)
        : nvs_erase_key(s_handle, "record");
    free(previous);
    if (rollback == ESP_ERR_NVS_NOT_FOUND && !had_previous) rollback = ESP_OK;
    if (rollback == ESP_OK) rollback = nvs_commit(s_handle);
    /* Never claim readable/persistent state after an unrecoverable rollback. */
    if (rollback != ESP_OK) passport_nvs_close();
    return PASSPORT_ERR_STORAGE;
}
esp_err_t passport_nvs_open(passport_store_backend_t *backend) {
    if (!backend) return ESP_ERR_INVALID_ARG;
    if (!s_open) {
        esp_err_t e = nvs_open("passport", NVS_READWRITE, &s_handle);
        if (e != ESP_OK) return e;
        s_open = true;
    }
    *backend = (passport_store_backend_t){.ctx=NULL,.read=read_blob,.write=write_blob};
    return ESP_OK;
}
void passport_nvs_close(void) {
    if (s_open) { nvs_close(s_handle); s_open=false; }
}
