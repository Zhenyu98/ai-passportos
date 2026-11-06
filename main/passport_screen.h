#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Used when the firmware has no stored idle setting of its own. */
#define PASSPORT_SCREEN_TIMEOUT_DEFAULT_S 120u

/* Owned by the UI worker. Pure timing logic: no ESP-IDF or LVGL. */
typedef struct {
    bool off;
    uint64_t last_activity_ms;
} passport_screen_t;

void passport_screen_init(passport_screen_t *screen, uint64_t now_ms);
void passport_screen_note_activity(passport_screen_t *screen, uint64_t now_ms);
/* True once a lit screen has idled for timeout_s. Zero disables the timer. */
bool passport_screen_due(const passport_screen_t *screen, uint64_t now_ms, unsigned timeout_s);

/* Firmware hooks with weak defaults; the Muse build supplies stored settings
 * and radio power. Called only from the UI worker. */
unsigned passport_screen_timeout_s(void);
void passport_screen_power_save(bool on);
