#include "passport_screen.h"

void passport_screen_init(passport_screen_t *screen, uint64_t now_ms) {
    screen->off = false;
    screen->last_activity_ms = now_ms;
}

void passport_screen_note_activity(passport_screen_t *screen, uint64_t now_ms) {
    screen->last_activity_ms = now_ms;
}

bool passport_screen_due(const passport_screen_t *screen, uint64_t now_ms, unsigned timeout_s) {
    if (screen->off || !timeout_s || now_ms < screen->last_activity_ms) return false;
    return now_ms - screen->last_activity_ms >= (uint64_t)timeout_s * 1000u;
}

__attribute__((weak)) unsigned passport_screen_timeout_s(void) {
    return PASSPORT_SCREEN_TIMEOUT_DEFAULT_S;
}

__attribute__((weak)) void passport_screen_power_save(bool on) {
    (void)on;
}
