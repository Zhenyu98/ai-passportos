#pragma once
#include "passport_store.h"
#include "esp_err.h"

/* NVS must already be initialized. No erase/recovery is attempted here. */
esp_err_t passport_nvs_open(passport_store_backend_t *backend);
void passport_nvs_close(void);
