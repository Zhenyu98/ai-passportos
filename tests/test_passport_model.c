#include "passport_model.h"
#include <assert.h>
#include <stdio.h>

static passport_action_t click(passport_model_t *m, passport_key_t key, uint64_t at) {
    assert(passport_model_input(m, key, PASSPORT_KEY_PRESS, at) == PASSPORT_ACTION_NONE);
    return passport_model_input(m, key, PASSPORT_KEY_RELEASE, at + 40);
}

int main(void) {
    passport_model_t m;
    passport_model_init(&m, 60);
    assert(m.page == PASSPORT_PAGE_BADGE && m.brightness == 60 && m.badge_button == 0);
    /* Home holds open settings independently of focus, exactly once. */
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_PRESS, 1);
    assert(passport_model_tick(&m, 2000) == PASSPORT_ACTION_NONE);
    assert(passport_model_tick(&m, 2001) == PASSPORT_ACTION_RENDER);
    assert(m.page == PASSPORT_PAGE_MENU);
    assert(passport_model_tick(&m, 4001) == PASSPORT_ACTION_NONE);
    assert(m.page == PASSPORT_PAGE_MENU);
    assert(passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 4010) == PASSPORT_ACTION_NONE);
    assert(m.page == PASSPORT_PAGE_MENU && m.menu_index == 0);
    passport_model_init(&m, 60);
    /* UP/DOWN move the AGENT/QR highlight; the page stays on the badge. */
    click(&m, PASSPORT_KEY_UP, 10);
    assert(m.page == PASSPORT_PAGE_BADGE && m.badge_button == 1);
    click(&m, PASSPORT_KEY_DOWN, 100);
    assert(m.badge_button == 0);
    click(&m, PASSPORT_KEY_DOWN, 200);
    assert(m.badge_button == 1);
    /* OK on the QR button opens the full-screen picture page. */
    click(&m, PASSPORT_KEY_OK, 300);
    assert(m.page == PASSPORT_PAGE_PICTURE);
    /* OK on the picture page returns to the badge (highlight stays on QR). */
    click(&m, PASSPORT_KEY_OK, 400);
    assert(m.page == PASSPORT_PAGE_BADGE && m.badge_button == 1);
    /* AGENT is independent of settings; exit and use a home hold for menu. */
    click(&m, PASSPORT_KEY_DOWN, 500);
    assert(m.badge_button == 0);
    click(&m, PASSPORT_KEY_OK, 600);
    assert(m.page == PASSPORT_PAGE_AGENT);
    assert(click(&m, PASSPORT_KEY_OK, 650) == PASSPORT_ACTION_AGENT_TOGGLE);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_PRESS, 660);
    assert(passport_model_tick(&m, 2660) == PASSPORT_ACTION_AGENT_LEAVE);
    assert(m.page == PASSPORT_PAGE_BADGE);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 2670);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_PRESS, 2700);
    assert(passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 4700) == PASSPORT_ACTION_RENDER);
    assert(m.page == PASSPORT_PAGE_MENU && m.menu_index == 0);
    /* The menu has five rows: UP from the first row wraps to Return. */
    click(&m, PASSPORT_KEY_UP, 700);
    assert(m.menu_index == 6);
    click(&m, PASSPORT_KEY_DOWN, 800);
    assert(m.menu_index == 0);
    /* Row 0: brightness. */
    click(&m, PASSPORT_KEY_OK, 900);
    assert(m.page == PASSPORT_PAGE_BRIGHTNESS);
    assert(click(&m, PASSPORT_KEY_DOWN, 1000) == PASSPORT_ACTION_SAVE_BRIGHTNESS);
    assert(m.brightness == 90);
    click(&m, PASSPORT_KEY_UP, 1100); assert(m.brightness == 60);
    click(&m, PASSPORT_KEY_UP, 1200); assert(m.brightness == 30);
    click(&m, PASSPORT_KEY_UP, 1300); assert(m.brightness == 90);
    click(&m, PASSPORT_KEY_OK, 1400);
    assert(m.page == PASSPORT_PAGE_MENU);
    /* Row 1: about; OK returns to the menu. */
    click(&m, PASSPORT_KEY_DOWN, 1500);
    click(&m, PASSPORT_KEY_OK, 1600);
    assert(m.page == PASSPORT_PAGE_ABOUT);
    click(&m, PASSPORT_KEY_OK, 1700);
    assert(m.page == PASSPORT_PAGE_MENU);
    /* Row 2 starts Wi-Fi configuration; row 3 opens the game. */
    click(&m, PASSPORT_KEY_DOWN, 1800);
    assert(m.menu_index == 2);
    assert(click(&m, PASSPORT_KEY_OK, 1900) == PASSPORT_ACTION_ENTER_CONFIG);
    assert(m.page == PASSPORT_PAGE_MENU);
    click(&m, PASSPORT_KEY_DOWN, 2000);
    assert(m.menu_index == 3);
    click(&m, PASSPORT_KEY_OK, 2100);
    assert(m.page == PASSPORT_PAGE_GAME);
    /* In the game, short clicks stay local; raw edges go to the UI. */
    assert(click(&m, PASSPORT_KEY_UP, 2200) == PASSPORT_ACTION_NONE);
    assert(click(&m, PASSPORT_KEY_OK, 2300) == PASSPORT_ACTION_NONE);
    assert(m.page == PASSPORT_PAGE_GAME);
    /* Long OK exits the game to the badge. */
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_PRESS, 2350);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_PRESS, 2400);
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_RELEASE, 2500);
    assert(passport_model_tick(&m, 4400) == PASSPORT_ACTION_RENDER);
    assert(m.page == PASSPORT_PAGE_BADGE);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 4450);
    /* Long OK returns home from the menu as well. */
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_PRESS, 4500);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 6500);
    assert(m.page == PASSPORT_PAGE_MENU);
    passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_PRESS, 5000);
    assert(passport_model_tick(&m, 6999) == PASSPORT_ACTION_NONE);
    assert(m.page == PASSPORT_PAGE_MENU);
    assert(passport_model_tick(&m, 7000) == PASSPORT_ACTION_RENDER);
    assert(m.page == PASSPORT_PAGE_BADGE);
    assert(passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 7020) == PASSPORT_ACTION_NONE);
    /* Non press/release key events are no-ops on the badge highlight. */
    click(&m, PASSPORT_KEY_DOWN, 8000);
    assert(m.badge_button == 1);
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_CLICK, 8100);
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_DOUBLE, 8110);
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_LONG, 8120);
    assert(m.badge_button == 1);
    passport_model_input(&m, PASSPORT_KEY_UP, PASSPORT_KEY_PRESS, 9000);
    assert(passport_model_tick(&m, 19000) == PASSPORT_ACTION_NONE);
    passport_model_input(&m, PASSPORT_KEY_UP, PASSPORT_KEY_RELEASE, 19010);
    assert(m.badge_button == 1);
    click(&m, PASSPORT_KEY_OK, 20000);
    assert(m.page == PASSPORT_PAGE_PICTURE);
    click(&m, PASSPORT_KEY_OK, 20100);
    assert(m.page == PASSPORT_PAGE_BADGE);
    passport_model_init(&m, 42);
    assert(m.brightness == 60 && m.badge_button == 0);
    assert(passport_model_input(&m, PASSPORT_KEY_OK, PASSPORT_KEY_RELEASE, 21000) == PASSPORT_ACTION_NONE);
    click(&m, PASSPORT_KEY_DOWN, 22000);
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_CLICK, 22050);
    passport_model_input(&m, PASSPORT_KEY_DOWN, PASSPORT_KEY_DOUBLE, 22150);
    assert(m.page == PASSPORT_PAGE_BADGE && m.badge_button == 1);
    /* Re-pairing needs a separate, cancel-by-default physical confirmation. */
    m.page=PASSPORT_PAGE_MUSE_SETUP;m.setup_choice=1;
    assert(click(&m,PASSPORT_KEY_OK,22500)==PASSPORT_ACTION_RENDER);
    assert(m.setup_confirm && m.setup_choice==0);
    assert(click(&m,PASSPORT_KEY_OK,22600)==PASSPORT_ACTION_RENDER);
    assert(!m.setup_confirm);
    m.setup_choice=1;click(&m,PASSPORT_KEY_OK,22700);
    click(&m,PASSPORT_KEY_DOWN,22800);
    assert(click(&m,PASSPORT_KEY_OK,22900)==PASSPORT_ACTION_MUSE_REPAIR);
    assert(!m.setup_confirm);
    puts("Passport model: PASS");
    return 0;
}
