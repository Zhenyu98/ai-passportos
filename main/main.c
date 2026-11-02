#include "passport_app.h"
#include "esp_log.h"

void app_main(void) {
    esp_err_t error=passport_app_start();
    if(error!=ESP_OK) ESP_LOGE("passport","Application startup failed: %s",esp_err_to_name(error));
}
