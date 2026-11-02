#include "passport_game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Host tests for the SMB-style platformer core. Positions are checked in
 * whole pixels (Q8 >> 8); physics windows are generous to stay robust when
 * the tuning constants move. warp() pins the camera mid-screen and disables
 * the level's scripted spawns so each mechanic is tested in isolation. */

static void run(passport_game_t *g, unsigned steps, uint32_t dt) {
    for (unsigned i = 0; i < steps; i++) passport_game_step(g, dt);
}
static void teleport(passport_game_t *g, int x, int y) {
    g->px_q8 = (int32_t)x << 8;
    g->py_q8 = (int32_t)y << 8;
    g->pvx_q8 = 0;
    g->pvy_q8 = 0;
    g->grounded = true;
}
static void warp(passport_game_t *g, int x, int y) {
    teleport(g, x, y);
    int cam = x - 120;
    if (cam < 0) cam = 0;
    if (cam > PASSPORT_GAME_LEVEL_W - PASSPORT_GAME_VIEW_W)
        cam = PASSPORT_GAME_LEVEL_W - PASSPORT_GAME_VIEW_W;
    g->cam_q8 = (int32_t)cam << 8;
    memset(g->goombas, 0, sizeof g->goombas);
    memset(g->coin, 0, sizeof g->coin);
    memset(g->popcoin, 0, sizeof g->popcoin);
    memset(g->frag, 0, sizeof g->frag);
    memset(&g->mushroom, 0, sizeof g->mushroom);
    g->goomba_spawn_idx = 1000; /* no scripted spawns during isolation */
    g->coin_spawn_idx = 1000;
    g->time_acc_ms = 0;
}
static int apex_of(passport_game_t *g, bool hold) {
    passport_game_jump(g, true);
    if (!hold) passport_game_jump(g, false);
    int apex = g->py_q8 >> 8;
    for (unsigned i = 0; i < 40; i++) {
        passport_game_step(g, 50);
        int y = g->py_q8 >> 8;
        if (y < apex) apex = y;
        if (g->grounded) break;
    }
    if (hold) passport_game_jump(g, false);
    return apex;
}

static void fresh_game(passport_game_t *g) {
    memset(g, 0, sizeof *g);
    passport_game_init(g, 42);
    passport_game_jump(g, true);
    g->goomba_spawn_idx = g->coin_spawn_idx = 1000;
}

static void test_first_surface_landing(void) {
    passport_game_t g;
    const unsigned frame_ms[] = {51, 52, 60};
    for (unsigned d = 0; d < sizeof frame_ms / sizeof frame_ms[0]; d++) {
        memset(&g, 0, sizeof g);
        passport_game_init(&g, 42);
        passport_game_jump(&g, true);
        g.cam_q8 = 32 * 256; /* first authored enemy enters the spawn window */
        g.invuln_ms = 60000;
        for (unsigned frame = 0; frame < 60; frame++) {
            passport_game_step(&g, frame_ms[d]);
            if (g.goombas[0].active) assert((g.goombas[0].y_q8 >> 8) + 14 <= 208);
        }
    }
    fresh_game(&g);
    g.goombas[0] = (passport_game_goomba_t){.active=true, .x_q8=160*256,
        .y_q8=192*256, .vy_q8=380*256, .dir=-1};
    passport_game_step(&g, 60);
    assert((g.goombas[0].y_q8 >> 8) + 14 == 208);
    run(&g, 4, 60);
    assert((g.goombas[0].y_q8 >> 8) + 14 == 208);
    fresh_game(&g);
    g.mushroom = (passport_game_mushroom_t){.active=true, .x_q8=160*256,
        .y_q8=192*256, .vy_q8=380*256, .dir=1};
    passport_game_step(&g, 60);
    assert((g.mushroom.y_q8 >> 8) + 14 == 208);
}

static void test_fractional_steering(void) {
    passport_game_t left, right;
    fresh_game(&left); fresh_game(&right);
    teleport(&left, 120, 193); teleport(&right, 120, 193);
    left.pvx_q8 = -120 * 256; right.pvx_q8 = 120 * 256;
    passport_game_left(&left, true); passport_game_right(&right, true);
    run(&left, 10, 16); run(&right, 10, 16);
    assert((right.px_q8 >> 8) == 139); /* 120 + 120 px/s * 0.16 s */
    assert(left.px_q8 + right.px_q8 == 240 * 256);
}

static void test_jump_windows(void) {
    passport_game_t g;
    fresh_game(&g);
    teleport(&g, 1100, 193); /* four pixels of support before the first pit */
    g.pvx_q8 = 120 * 256;
    passport_game_right(&g, true);
    passport_game_step(&g, 50);
    assert(!g.grounded);
    passport_game_jump(&g, true);
    assert(g.pvy_q8 < 0 && !g.air_jump_used); /* grace preserves double jump */

    fresh_game(&g);
    teleport(&g, 69 * 16, 193);
    g.grounded = false;
    run(&g, 3, 50); /* ground grace has expired above the pit */
    passport_game_jump(&g, true);
    assert(g.air_jump_used);

    fresh_game(&g);
    teleport(&g, 120, 188);
    g.grounded = false; g.air_jump_used = true; g.pvy_q8 = 100 * 256;
    g.ground_grace_ms = 0; /* this fall began well before the landing window */
    passport_game_jump(&g, true); /* spent jump waits for the imminent landing */
    passport_game_step(&g, 50);
    assert(!g.grounded && g.pvy_q8 < 0 && !g.air_jump_used);

    fresh_game(&g);
    teleport(&g, 120, 70);
    g.grounded = false; g.air_jump_used = true; g.pvy_q8 = 100 * 256;
    g.ground_grace_ms = 0;
    passport_game_jump(&g, true);
    run(&g, 12, 50); /* too early: buffer must expire before landing */
    assert(g.grounded && g.pvy_q8 == 0);
}

static void test_direction_handoff(void) {
    passport_game_t g;
    fresh_game(&g);
    teleport(&g, 160, 193);
    passport_game_left(&g, true);
    run(&g, 8, 50);
    passport_game_left(&g, false);
    passport_game_step(&g, 50); /* a release-to-OK gap loses ground speed */
    passport_game_jump(&g, true);
    assert(g.pvx_q8 == -120 * 256 && g.pvy_q8 < 0);

    fresh_game(&g);
    passport_game_right(&g, true); run(&g, 8, 50);
    passport_game_right(&g, false); run(&g, 4, 50);
    passport_game_jump(&g, true);
    assert(g.pvx_q8 == 0); /* stale direction must not restart movement */

    fresh_game(&g);
    teleport(&g, 160, 193);
    passport_game_left(&g, true); run(&g, 8, 50);
    passport_game_left(&g, false);
    passport_game_right(&g, true); passport_game_step(&g, 50);
    int32_t turning_vx = g.pvx_q8;
    passport_game_jump(&g, true);
    assert(g.pvx_q8 == turning_vx && g.right_held);
}

static void test_input_reset(void) {
    passport_game_t g;
    fresh_game(&g);
    teleport(&g, 120, 188);
    g.grounded = false; g.ground_grace_ms = 0;
    g.air_jump_used = true; g.pvy_q8 = 100 * 256;
    passport_game_right(&g, true);
    passport_game_jump(&g, true); /* pending jump must not survive input loss */
    passport_game_reset_input(&g);
    passport_game_step(&g, 50);
    assert(g.px_q8 == 120 * 256 && !g.left_held && !g.right_held);
    assert(g.grounded && g.pvy_q8 == 0);
}

static void test_stomp_jump_budget(void) {
    passport_game_t g;
    fresh_game(&g);
    teleport(&g, 120, 190);
    g.grounded=false; g.ground_grace_ms=0; g.pvy_q8=100*256;
    g.goombas[0]=(passport_game_goomba_t){.active=true, .x_q8=120*256,
        .y_q8=194*256, .dir=-1};
    passport_game_step(&g,50); /* floor landing and stomp in the same frame */
    assert(g.goombas[0].squash_ms && g.pvy_q8<0 && !g.grounded);
    passport_game_jump(&g,true);
    assert(g.air_jump_used); /* exactly one extra jump after the bounce */
    passport_game_step(&g,20);
    int32_t vy=g.pvy_q8;
    passport_game_jump(&g,true);
    assert(g.pvy_q8==vy); /* a third impulse cannot be injected */
}

static void test_held_direction_survives_respawn(void) {
    passport_game_t g;
    fresh_game(&g);
    teleport(&g, 69*16, 193);
    passport_game_right(&g,true); /* no new press edge after respawn */
    for(unsigned i=0;i<40 && g.state==PASSPORT_GAME_PLAYING;i++) passport_game_step(&g,50);
    assert(g.state==PASSPORT_GAME_DYING);
    for(unsigned i=0;i<40 && g.state==PASSPORT_GAME_DYING;i++) passport_game_step(&g,50);
    assert(g.state==PASSPORT_GAME_PLAYING);
    passport_game_step(&g,50);
    assert(g.pvx_q8>0 && g.px_q8>32*256);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        if (!strcmp(argv[1], "landing")) test_first_surface_landing();
        else if (!strcmp(argv[1], "fractional")) test_fractional_steering();
        else if (!strcmp(argv[1], "jump-windows")) test_jump_windows();
        else if (!strcmp(argv[1], "handoff")) test_direction_handoff();
        else if (!strcmp(argv[1], "reset")) test_input_reset();
        else if (!strcmp(argv[1], "stomp")) test_stomp_jump_budget();
        else if (!strcmp(argv[1], "respawn")) test_held_direction_survives_respawn();
        else return 2;
        puts("game regression: PASS");
        return 0;
    }
    test_first_surface_landing();
    test_fractional_steering();
    test_jump_windows();
    test_direction_handoff();
    test_input_reset();
    test_stomp_jump_budget();
    test_held_direction_survives_respawn();
    passport_game_t g;

    /* Init: READY on the ground at the spawn point; best survives resets. */
    memset(&g, 0xff, sizeof g);
    passport_game_init(&g, 42);
    assert(g.state == PASSPORT_GAME_READY);
    assert(g.grounded && g.lives == 3 && g.time == 300 && g.loop == 1);
    assert((g.px_q8 >> 8) == 32 && (g.py_q8 >> 8) == 193);
    assert(g.tile[13][2] == PASSPORT_TILE_GROUND && g.tile[14][2] == PASSPORT_TILE_GROUND);
    assert(g.tile[13][69] == PASSPORT_TILE_EMPTY); /* first pit */
    assert(g.tile[9][16] == PASSPORT_TILE_QCOIN && g.tile[9][23] == PASSPORT_TILE_QMUSH);
    assert(g.pipe_count == 5 && g.pipes[2].col == 46 && g.pipes[2].height == 4);
    assert(g.flag_col == 194 && g.castle_col == 200);
    g.best = 777;
    passport_game_init(&g, 43);
    assert(g.best == 777);

    /* READY ignores stepping; the first OK press starts (no jump). */
    run(&g, 20, 50);
    assert(g.state == PASSPORT_GAME_READY && g.cam_q8 == 0);
    passport_game_jump(&g, true);
    assert(g.state == PASSPORT_GAME_PLAYING && g.grounded && g.pvy_q8 == 0);
    passport_game_jump(&g, false);

    /* Run right: accelerates to the walk cap, friction stops it on release. */
    passport_game_right(&g, true);
    run(&g, 20, 50);
    assert(g.pvx_q8 == 120 * 256);
    assert((g.px_q8 >> 8) > 32 && g.facing == 1);
    passport_game_right(&g, false);
    run(&g, 6, 50);
    assert(g.pvx_q8 == 0 && g.grounded);

    /* Jump: a fixed full-height arc; releasing OK does not cut it. */
    warp(&g, 120, 193);
    int hold_apex = apex_of(&g, true);
    int tap_apex = apex_of(&g, false);
    assert(hold_apex >= 88 && hold_apex <= 106);
    assert(tap_apex == hold_apex); /* fixed jump: the release is a no-op */
    assert(g.grounded && (g.py_q8 >> 8) == 193);

    /* Double jump: one extra impulse mid-air, then it is spent. */
    warp(&g, 120, 193);
    passport_game_jump(&g, true); /* ground jump */
    run(&g, 2, 50);               /* rising */
    assert(!g.grounded && !g.air_jump_used);
    passport_game_jump(&g, true); /* air jump */
    assert(g.air_jump_used && g.pvy_q8 == -440 * 256);
    passport_game_jump(&g, true); /* already spent: ignored */
    assert(g.pvy_q8 == -440 * 256);
    run(&g, 40, 50);              /* land */
    assert(g.grounded && !g.air_jump_used);

    /* Air momentum: jump, release direction input, keep drifting. */
    passport_game_right(&g, true);
    run(&g, 6, 50);
    int32_t vx = g.pvx_q8;
    passport_game_jump(&g, true);
    passport_game_right(&g, false); /* ladder: OK press hides the key */
    run(&g, 6, 50);
    assert(!g.grounded && g.pvx_q8 == vx); /* no air friction without input */
    passport_game_jump(&g, false);
    run(&g, 40, 50);
    assert(g.grounded);

    /* Head-bump a question block: coin pops, block is spent. */
    warp(&g, 16 * 16 + 2, 193);
    passport_game_jump(&g, true);
    run(&g, 6, 50);
    assert(g.tile[9][16] == PASSPORT_TILE_USED);
    assert(g.coins == 1 && g.score == 200);
    assert(g.popcoin[0].active || g.popcoin[1].active); /* 420 ms lifetime */
    passport_game_jump(&g, false);
    run(&g, 34, 50);

    /* Bricks break from below with flying shards. */
    warp(&g, 20 * 16 + 2, 193);
    passport_game_jump(&g, true);
    run(&g, 10, 50);
    assert(g.tile[9][20] == PASSPORT_TILE_EMPTY && g.score == 250);
    assert(g.frag[0].active && g.frag[3].active);
    passport_game_jump(&g, false);
    run(&g, 40, 50);

    /* Static map coins are collected on contact. */
    warp(&g, 33 * 16 + 2, 9 * 16 - 6); /* overlapping the coin's row */
    g.coin[0].active = true;
    g.coin[0].x_q8 = (int32_t)(33 * 16 + 2) << 8;
    g.coin[0].y = 9 * 16 + 2;
    run(&g, 1, 50);
    assert(!g.coin[0].active && g.coins == 2 && g.score == 250 + 200);

    /* Mushroom: emerges, walks off the block, then grows the player. */
    warp(&g, 23 * 16 + 2, 193);
    passport_game_jump(&g, true);
    run(&g, 6, 50);
    assert(g.tile[9][23] == PASSPORT_TILE_USED);
    assert(g.mushroom.active && g.mushroom.emerging);
    passport_game_jump(&g, false);
    teleport(&g, 300, 193); /* step aside so the mushroom can walk on */
    run(&g, 30, 50); /* 1.5 s: emerge, cross to the block edge, land */
    assert(g.mushroom.active && !g.mushroom.emerging);
    assert((g.mushroom.y_q8 >> 8) == 194); /* back on the ground */
    teleport(&g, g.mushroom.x_q8 >> 8, 193); /* feet exactly on the floor */
    run(&g, 2, 50);
    assert(!g.mushroom.active && g.big && g.score >= 1000 + 450);
    assert((g.py_q8 >> 8) + 27 == 208); /* feet planted after growth */

    /* Big player shrinks with invulnerability; the window expires. */
    memset(&g.goombas[0], 0, sizeof g.goombas[0]);
    g.goombas[0].active = true;
    g.goombas[0].dir = -1; /* walks away so the blink window can be timed */
    g.goombas[0].x_q8 = g.px_q8;
    g.goombas[0].y_q8 = (int32_t)(208 - 14) << 8;
    run(&g, 1, 50);
    assert(!g.big && g.invuln_ms > 0 && g.state == PASSPORT_GAME_PLAYING);
    run(&g, 1, 50); /* still blinking: no death */
    assert(g.state == PASSPORT_GAME_PLAYING);
    run(&g, 45, 50); /* past the 2s window */
    assert(g.invuln_ms == 0);

    /* Stomp: squash, bounce and score. */
    memset(&g.goombas[0], 0, sizeof g.goombas[0]);
    g.goombas[0].active = true;
    g.goombas[0].dir = -1;
    g.goombas[0].x_q8 = g.px_q8;
    g.goombas[0].y_q8 = (int32_t)(208 - 14) << 8;
    teleport(&g, g.px_q8 >> 8, 182);
    g.pvy_q8 = 100 * 256;
    g.grounded = false;
    uint32_t score_before = g.score;
    run(&g, 1, 50);
    assert(g.goombas[0].squash_ms > 0);
    assert(g.score == score_before + 100 && g.pvy_q8 < 0);
    run(&g, 10, 50);
    assert(!g.goombas[0].active); /* faded out */

    /* Side contact while small: death arc, respawn, level rebuilt. */
    memset(&g.goombas[0], 0, sizeof g.goombas[0]);
    g.goombas[0].active = true;
    g.goombas[0].dir = -1;
    g.goombas[0].x_q8 = g.px_q8;
    g.goombas[0].y_q8 = (int32_t)(208 - 14) << 8;
    teleport(&g, g.px_q8 >> 8, 193);
    g.pvy_q8 = 0;
    run(&g, 1, 50);
    assert(g.state == PASSPORT_GAME_DYING && g.lives == 3);
    run(&g, 40, 50); /* 2s > death arc */
    assert(g.state == PASSPORT_GAME_PLAYING && g.lives == 2);
    assert((g.px_q8 >> 8) == 32 && (g.py_q8 >> 8) == 193);
    assert(g.tile[9][20] == PASSPORT_TILE_BRICK); /* rebuilt */
    assert(g.time >= 298 && g.time <= 300); /* fresh clock, some drift */

    /* Pit: falls past the floor and dies. */
    warp(&g, 69 * 16 + 4, 193);
    run(&g, 30, 50);
    assert(g.state == PASSPORT_GAME_DYING);
    run(&g, 40, 50);
    assert(g.state == PASSPORT_GAME_PLAYING && g.lives == 1);

    /* Pipes block horizontal movement. */
    warp(&g, 27 * 16 + 6, 193);
    g.pvx_q8 = 95 * 256;
    passport_game_right(&g, true);
    run(&g, 4, 50);
    assert((g.px_q8 >> 8) == 28 * 16 - 12 && g.pvx_q8 == 0);
    passport_game_right(&g, false);

    /* Camera: follows right, never scrolls back, walls the player on the
     * left (classic). */
    warp(&g, 1000, 193);
    g.cam_q8 = 0;
    run(&g, 1, 50);
    assert(g.cam_q8 == (1000 - 120) * 256);
    teleport(&g, 500, 193); /* camera stays where it was */
    passport_game_left(&g, true);
    run(&g, 3, 50);
    assert(g.cam_q8 == (1000 - 120) * 256); /* never scrolls back */
    assert((g.px_q8 >> 8) == (1000 - 120)); /* classic left wall */
    passport_game_left(&g, false);

    /* Time out kills and exhausts the last life. */
    warp(&g, 40, 193);
    g.time = 1;
    g.time_acc_ms = 390;
    passport_game_step(&g, 50);
    assert(g.state == PASSPORT_GAME_DYING);
    run(&g, 40, 50);
    assert(g.state == PASSPORT_GAME_OVER && g.lives == 0);
    assert(g.best >= g.score);

    /* Restart from OVER keeps the best score. */
    g.best = 5000;
    passport_game_jump(&g, true);
    assert(g.state == PASSPORT_GAME_READY && g.best == 5000);
    assert(g.score == 0 && g.lives == 3 && g.time == 300);
    passport_game_jump(&g, false);

    /* Flag: slide, walk, pose, time bonus, next loop. */
    passport_game_jump(&g, true);
    assert(g.state == PASSPORT_GAME_PLAYING);
    warp(&g, 194 * 16 - 24, 193);
    passport_game_right(&g, true);
    run(&g, 10, 50);
    assert(g.state == PASSPORT_GAME_CLEAR && g.clear_phase != PASSPORT_CLEAR_POSE);
    passport_game_right(&g, false);
    run(&g, 90, 50); /* slide + walk to the castle door */
    assert(g.clear_phase == PASSPORT_CLEAR_POSE);
    assert(g.time == 0); /* bonus banked */
    run(&g, 20, 50);
    passport_game_jump(&g, true);
    passport_game_jump(&g, false);
    assert(g.state == PASSPORT_GAME_PLAYING && g.loop == 2 && g.time == 275);
    assert((g.px_q8 >> 8) == 32 && g.cam_q8 == 0);

    /* dt is clamped to 60 ms. */
    warp(&g, 40, 193);
    passport_game_right(&g, true);
    passport_game_step(&g, 1000);
    assert((g.px_q8 >> 8) - 40 <= 6); /* 800 px/s^2 * 0.06 s ~= 48 px/s */
    passport_game_right(&g, false);

    /* Determinism: identical scripted runs agree bit for bit. */
    passport_game_t a, b;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    passport_game_init(&a, 7);
    passport_game_init(&b, 7);
    passport_game_jump(&a, true); passport_game_jump(&b, true);
    for (unsigned i = 0; i < 400; i++) {
        bool right = (i / 20) % 2 == 0, jump = i % 16 == 0;
        passport_game_right(&a, right); passport_game_right(&b, right);
        if (jump) { passport_game_jump(&a, true); passport_game_jump(&b, true); }
        if (i % 16 == 2) { passport_game_jump(&a, false); passport_game_jump(&b, false); }
        passport_game_step(&a, 50);
        passport_game_step(&b, 50);
    }
    assert(memcmp(&a, &b, sizeof a) == 0);

    printf("passport game: all assertions passed\n");
    return 0;
}
