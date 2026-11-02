#pragma once
#include "passport_model.h"
#include "esp_err.h"
/* Start/stop have one lifecycle owner. Stop retains resources on timeout. */
esp_err_t passport_app_start(void);
esp_err_t passport_app_stop(void);
bool passport_app_post_input(passport_key_t,passport_key_event_t,uint64_t now_ms);
/* Accepted means queued for the UI owner, not already persisted/applied. */
bool passport_app_remote_brightness(unsigned percent);
bool passport_app_remote_page(passport_page_t);
void passport_app_device_status(int *battery,unsigned *brightness,passport_page_t *page);
/* Network time has set the system clock; safe from any task, stays set until reboot. */
void passport_app_clock_synced(void);
