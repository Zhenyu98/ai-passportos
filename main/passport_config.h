#pragma once
#include "passport_profile.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SoftAP configuration service: the badge hosts "Badge-XXXX" and serves a
 * mobile web page for editing the profile, setting the clock, and uploading
 * an avatar. Storage goes through the application's single-writer worker. */

typedef enum {
    PASSPORT_CONFIG_MEDIA_CHANGED,
    PASSPORT_CONFIG_STOPPED, PASSPORT_CONFIG_FAILED
} passport_config_event_t;

typedef struct {
    /* Serialized through the app core worker; may block up to ~2 s. */
    passport_status_t (*submit_profile)(const passport_profile_t *profile);
    passport_status_t (*submit_time)(int64_t utc_epoch_s);
    /* Called from the config task; take the LVGL lock before touching UI. */
    void (*on_event)(passport_config_event_t event);
    /* Profile shown on GET /api/profile at session start. */
    passport_profile_t snapshot;
    /* Muse owns the connected station; serve HTTP without changing its radio. */
    bool shared_station;
    char station_address[16];
} passport_config_api_t;

esp_err_t passport_config_start(const passport_config_api_t *api);
esp_err_t passport_config_stop(void);
bool passport_config_running(void);
void passport_config_ssid(char *buffer, size_t size);
/* Temporary LAN editor password; displayed on the device, never logged. */
bool passport_config_password(char *buffer,size_t size);
