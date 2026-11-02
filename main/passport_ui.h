#pragma once
#include "passport_model.h"
#include "passport_store.h"
#include "passport_agent_model.h"
#include <stddef.h>
/* All calls require the LVGL lock (or the LVGL owner task). */
bool passport_ui_create(void);
bool passport_ui_render(const passport_model_t *,const passport_record_t *,int battery_soc,
                        bool time_valid,int64_t utc_epoch_s);
void passport_ui_status(const char *text);
/* A real successful operation; ignored while no animated avatar is visible. */
void passport_ui_acknowledge(void);
void passport_ui_agent_snapshot(const passport_agent_model_t *);
void passport_ui_agent_scroll(int direction);
void passport_ui_configuration(const char *address,const char *password);
void passport_ui_hotspot(const char *ssid);
/* Raw key edges for the game page. Touches only game state (no LVGL), so the
 * UI owner task may call it without holding the LVGL lock. */
void passport_ui_game_key(passport_key_t key,passport_key_event_t event);
/* Same owner/context as game_key: clear held inputs and queued game actions. */
void passport_ui_game_reset_input(void);
size_t passport_ui_page_buffers(void);
void passport_ui_destroy(void);
