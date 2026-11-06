#include "passport_model.h"
#include <string.h>

void passport_model_reset_input(passport_model_t *m) {
    m->held_key = -1; m->long_fired = false; m->pressed_at = 0;
}

void passport_model_init(passport_model_t *m, uint8_t brightness) {
    memset(m, 0, sizeof *m);
    m->page = PASSPORT_PAGE_BADGE; m->return_page = PASSPORT_PAGE_BADGE;
    m->brightness = passport_brightness_valid(brightness) ? brightness : PASSPORT_BRIGHTNESS_DEFAULT;
    passport_model_reset_input(m);
}

static passport_action_t home(passport_model_t *m) {
    bool leaving_agent = m->page == PASSPORT_PAGE_AGENT;
    m->page = PASSPORT_PAGE_BADGE;
    return leaving_agent ? PASSPORT_ACTION_AGENT_LEAVE : PASSPORT_ACTION_RENDER;
}

static passport_action_t long_ok(passport_model_t *m) {
    if (m->page != PASSPORT_PAGE_BADGE) return home(m);
    m->return_page = PASSPORT_PAGE_BADGE;
    m->menu_index = 0;
    m->page = PASSPORT_PAGE_MENU;
    return PASSPORT_ACTION_RENDER;
}

static passport_action_t on_click(passport_model_t *m, passport_key_t key) {
    if (m->page == PASSPORT_PAGE_BADGE) {
        /* UP/DOWN move the highlight between the two bottom buttons; OK enters. */
        if (key == PASSPORT_KEY_UP || key == PASSPORT_KEY_DOWN) {
            m->badge_button ^= 1u;
            return PASSPORT_ACTION_RENDER;
        }
        if (m->badge_button == 0) {
            m->page = PASSPORT_PAGE_AGENT;
        } else { /* QR: full-screen picture */
            m->page = PASSPORT_PAGE_PICTURE;
        }
        return PASSPORT_ACTION_RENDER;
    }
    if (m->page == PASSPORT_PAGE_PICTURE) {
        if (key == PASSPORT_KEY_OK) { m->page = PASSPORT_PAGE_BADGE; return PASSPORT_ACTION_RENDER; }
        return PASSPORT_ACTION_NONE;
    }
    if (m->page == PASSPORT_PAGE_MENU) {
        if (key == PASSPORT_KEY_UP) m->menu_index = (m->menu_index + 6) % 7;
        else if (key == PASSPORT_KEY_DOWN) m->menu_index = (m->menu_index + 1) % 7;
        else if (m->menu_index == 0) m->page = PASSPORT_PAGE_BRIGHTNESS;
        else if (m->menu_index == 1) m->page = PASSPORT_PAGE_ABOUT;
        else if (m->menu_index == 2) return PASSPORT_ACTION_ENTER_CONFIG;
        else if (m->menu_index == 3) m->page = PASSPORT_PAGE_GAME;
        else if (m->menu_index == 4) m->page = PASSPORT_PAGE_TASKS;
        else if (m->menu_index == 5) m->page = PASSPORT_PAGE_NOTES;
        else m->page = m->return_page;
    } else if (m->page == PASSPORT_PAGE_MUSE_SETUP) {
        unsigned count=m->setup_confirm?2:3;
        if(key==PASSPORT_KEY_UP) m->setup_choice=(m->setup_choice+count-1)%count;
        else if(key==PASSPORT_KEY_DOWN) m->setup_choice=(m->setup_choice+1)%count;
        else if(m->setup_confirm) {
            bool confirmed=m->setup_choice==1;m->setup_confirm=false;m->setup_choice=1;
            if(confirmed) return PASSPORT_ACTION_MUSE_REPAIR;
        } else if(m->setup_choice==0) return PASSPORT_ACTION_ENTER_SHARED_CONFIG;
        else if(m->setup_choice==1) {m->setup_confirm=true;m->setup_choice=0;}
        else {m->page=PASSPORT_PAGE_MENU;m->menu_index=2;}
        return PASSPORT_ACTION_RENDER;
    } else if (m->page >= PASSPORT_PAGE_AGENT) {
        if (key == PASSPORT_KEY_UP) return PASSPORT_ACTION_SCROLL_UP;
        if (key == PASSPORT_KEY_DOWN) return PASSPORT_ACTION_SCROLL_DOWN;
        if (m->page == PASSPORT_PAGE_AGENT) return PASSPORT_ACTION_AGENT_TOGGLE;
        if (m->page == PASSPORT_PAGE_TASKS) return PASSPORT_ACTION_SAVE_NOTE;
        return PASSPORT_ACTION_NONE;
    } else if (m->page == PASSPORT_PAGE_GAME) {
        /* The game reads raw press/release through the UI; clicks and the
         * long-OK home shortcut stay handled here. */
        return PASSPORT_ACTION_NONE;
    } else if (m->page == PASSPORT_PAGE_BRIGHTNESS) {
        if (key == PASSPORT_KEY_OK) m->page = PASSPORT_PAGE_MENU;
        else {
            /* UP brightens and DOWN dims; the ends stop rather than jump, and
             * a press that changes nothing does not write to Flash. */
            if (key == PASSPORT_KEY_UP) {
                if (m->brightness >= PASSPORT_BRIGHTNESS_MAX) return PASSPORT_ACTION_NONE;
                m->brightness += PASSPORT_BRIGHTNESS_STEP;
            } else {
                if (m->brightness <= PASSPORT_BRIGHTNESS_MIN) return PASSPORT_ACTION_NONE;
                m->brightness -= PASSPORT_BRIGHTNESS_STEP;
            }
            return PASSPORT_ACTION_SAVE_BRIGHTNESS;
        }
    } else if (m->page == PASSPORT_PAGE_ABOUT && key == PASSPORT_KEY_OK) {
        m->page = PASSPORT_PAGE_MENU;
    } else return PASSPORT_ACTION_NONE;
    return PASSPORT_ACTION_RENDER;
}

passport_action_t passport_model_tick(passport_model_t *m, uint64_t now) {
    if (!m || m->held_key < 0 || now < m->pressed_at) return PASSPORT_ACTION_NONE;
    uint64_t duration = now - m->pressed_at;
    if (duration >= 10000) { passport_model_reset_input(m); return PASSPORT_ACTION_NONE; }
    if (m->held_key == PASSPORT_KEY_OK && duration >= 2000 && !m->long_fired) {
        m->long_fired = true;
        return long_ok(m);
    }
    return PASSPORT_ACTION_NONE;
}

passport_action_t passport_model_input(passport_model_t *m, passport_key_t key,
                                       passport_key_event_t event, uint64_t now) {
    if (!m || key < PASSPORT_KEY_UP || key > PASSPORT_KEY_OK) return PASSPORT_ACTION_NONE;
    /* Gameplay consumes direction levels independently. Only OK owns the
     * global hold timer, even when DOWN+OK are physically pressed together. */
    if (m->page == PASSPORT_PAGE_GAME && key != PASSPORT_KEY_OK) return PASSPORT_ACTION_NONE;
    if (event == PASSPORT_KEY_PRESS) {
        if (m->held_key < 0) { m->held_key = key; m->pressed_at = now; m->long_fired = false; }
        return PASSPORT_ACTION_NONE;
    }
    if (event != PASSPORT_KEY_RELEASE || m->held_key != (int)key) return PASSPORT_ACTION_NONE;
    bool fired = m->long_fired;
    uint64_t duration = now >= m->pressed_at ? now - m->pressed_at : 10000;
    passport_model_reset_input(m);
    if (fired || duration >= 10000) return PASSPORT_ACTION_NONE;
    if (duration >= 2000) return key == PASSPORT_KEY_OK ? long_ok(m) : PASSPORT_ACTION_NONE;
    return on_click(m, key);
}
