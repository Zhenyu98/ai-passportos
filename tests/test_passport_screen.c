#include "passport_screen.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    passport_screen_t screen;
    passport_screen_init(&screen, 1000);
    assert(!screen.off && screen.last_activity_ms == 1000);

    /* The boundary is inclusive and measured from the latest activity. */
    assert(!passport_screen_due(&screen, 120999, 120));
    assert(passport_screen_due(&screen, 121000, 120));
    passport_screen_note_activity(&screen, 121000);
    assert(!passport_screen_due(&screen, 121001, 120));
    assert(passport_screen_due(&screen, 241000, 120));

    /* Zero means never; a dark screen is never due again. */
    assert(!passport_screen_due(&screen, 99999999, 0));
    screen.off = true;
    assert(!passport_screen_due(&screen, 99999999, 120));
    screen.off = false;

    /* A clock reading older than the activity is not treated as idle time. */
    passport_screen_note_activity(&screen, 5000);
    assert(!passport_screen_due(&screen, 4999, 1));

    /* The largest stored setting cannot overflow the millisecond comparison. */
    passport_screen_init(&screen, 0);
    assert(!passport_screen_due(&screen, 4294967295ull, 4294967295u));
    assert(passport_screen_due(&screen, 4294967295000ull, 4294967295u));

    /* Default firmware falls back to the weak hooks. */
    assert(passport_screen_timeout_s() == PASSPORT_SCREEN_TIMEOUT_DEFAULT_S);
    passport_screen_power_save(true);
    passport_screen_power_save(false);

    puts("Passport idle screen timing: PASS");
    return 0;
}
