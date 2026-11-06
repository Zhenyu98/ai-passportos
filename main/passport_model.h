#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Ten levels. Keep multiples of the step so records saved by older firmware
 * (30, 60 or 90) stay valid. */
#define PASSPORT_BRIGHTNESS_MIN 10u
#define PASSPORT_BRIGHTNESS_MAX 100u
#define PASSPORT_BRIGHTNESS_STEP 10u
#define PASSPORT_BRIGHTNESS_DEFAULT 60u
static inline bool passport_brightness_valid(unsigned percent) {
    return percent>=PASSPORT_BRIGHTNESS_MIN && percent<=PASSPORT_BRIGHTNESS_MAX &&
        percent%PASSPORT_BRIGHTNESS_STEP==0;
}

typedef enum {
    PASSPORT_PAGE_BADGE, PASSPORT_PAGE_PICTURE,
    PASSPORT_PAGE_MENU, PASSPORT_PAGE_BRIGHTNESS, PASSPORT_PAGE_ABOUT,
    PASSPORT_PAGE_GAME, PASSPORT_PAGE_AGENT, PASSPORT_PAGE_TASKS, PASSPORT_PAGE_NOTES,
    PASSPORT_PAGE_MUSE_SETUP
} passport_page_t;
typedef enum { PASSPORT_KEY_UP, PASSPORT_KEY_DOWN, PASSPORT_KEY_OK } passport_key_t;
typedef enum {
    PASSPORT_KEY_PRESS, PASSPORT_KEY_RELEASE, PASSPORT_KEY_CLICK,
    PASSPORT_KEY_DOUBLE, PASSPORT_KEY_LONG
} passport_key_event_t;
typedef enum {
    PASSPORT_ACTION_NONE, PASSPORT_ACTION_RENDER, PASSPORT_ACTION_SAVE_BRIGHTNESS,
    PASSPORT_ACTION_ENTER_CONFIG, PASSPORT_ACTION_AGENT_TOGGLE,
    PASSPORT_ACTION_AGENT_LEAVE, PASSPORT_ACTION_SCROLL_UP,
    PASSPORT_ACTION_SCROLL_DOWN, PASSPORT_ACTION_SAVE_NOTE,
    PASSPORT_ACTION_ENTER_SHARED_CONFIG, PASSPORT_ACTION_MUSE_REPAIR
} passport_action_t;
typedef struct {
    passport_page_t page, return_page;
    unsigned menu_index;
    unsigned badge_button; /* 0 = AGENT, 1 = QR, selected on the badge page. */
    uint8_t brightness;
    int held_key;
    uint64_t pressed_at;
    bool long_fired;
    unsigned setup_choice;
    bool setup_confirm;
} passport_model_t;

void passport_model_init(passport_model_t *model, uint8_t brightness);
passport_action_t passport_model_input(passport_model_t *model, passport_key_t key,
                                       passport_key_event_t event, uint64_t now_ms);
passport_action_t passport_model_tick(passport_model_t *model, uint64_t now_ms);
void passport_model_reset_input(passport_model_t *model);
