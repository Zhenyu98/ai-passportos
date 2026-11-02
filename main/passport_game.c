#include "passport_game.h"
#include <string.h>

/* Super-Mario-Bros-style platformer simulation. Deterministic Q8 fixed point
 * throughout; no RNG is needed because the level is authored, not rolled. */

#define Q(x) ((int32_t)(x) * 256)
#define TILE PASSPORT_GAME_TILE
#define ROWS PASSPORT_GAME_ROWS
#define COLS PASSPORT_GAME_COLS

/* Physics tuning (px/s and px/s^2; heights verified against the level). */
#define PLAYER_W 12
#define PLAYER_H_SMALL 15
#define PLAYER_H_BIG 27
#define WALK_MAX 120
#define ACCEL_GROUND 800
#define ACCEL_TURN 900     /* reversing direction skids harder */
#define ACCEL_AIR 600      /* steering while airborne stays responsive */
#define FRICTION 800       /* grounded, no input */
#define JUMP_VY (-440)     /* full-height arc, apex about 97 px */
#define GRAVITY_RISE 1000  /* rising: fixed jump, no hold-to-cut */
#define GRAVITY_FALL 1600  /* falling: floaty so pipes stay clearable */
#define TERMINAL 380
#define STOMP_BOUNCE (-430)
#define GOOMBA_SPEED_BASE 30
#define GOOMBA_SPEED_PER_LOOP 8
#define GOOMBA_SPEED_MAX 62
#define MUSHROOM_SPEED 45
#define MUSHROOM_RISE 32   /* emerging from the block, px/s */
#define POPCOIN_VY (-320)
#define POPCOIN_GRAV 1600
#define FRAG_GRAV 900
#define DEATH_VY (-350)
#define DEATH_MS 1600
#define TIME_START 300
#define TIME_UNIT_MS 400
#define CAMERA_ANCHOR 120  /* player is kept this far from the view's left */
#define FLAG_COL_POS 194
#define CASTLE_COL_POS 200
#define POLE_TOP_Y 48
#define POLE_BASE_Y 192
#define FLAG_REST_Y 184
#define CLEAR_SLIDE_VY 150
#define FLAG_SLIDE_VY 260
#define CLEAR_WALK_VX 60
#define GROUND_GRACE_MS 100
#define JUMP_BUFFER_MS 120
#define MOVE_GRACE_MS 120

/* ---- Authored level (SMB 1-1 spirit, original layout). ---- */
static const uint8_t GROUND_SEGS[][2] = {
    {0, 68}, {71, 85}, {89, 135}, {139, 211}
};
static const uint8_t PIPES[][2] = {
    {28, 2}, {38, 3}, {46, 4}, {57, 4}, {163, 2}
};
static const uint16_t GOOMBA_SPAWNS[] = {
    24, 40, 51, 53, 64, 97, 99, 107, 109, 114, 124, 126, 131, 150, 152, 166, 168
};
static const uint8_t COIN_SPAWNS[][2] = {
    {33, 9}, {34, 9}, {23, 5}, {51, 9}, {52, 9}, {64, 5}, {65, 5},
    {86, 8}, {87, 8}, {88, 8}, {105, 8}, {120, 8}, {121, 8},
    {136, 8}, {137, 8}, {138, 8}, {150, 5}, {151, 5}, {152, 5},
    {171, 4}, {172, 4}
};

static void set_tile(passport_game_t *g, int row, int col, uint8_t t) {
    if (row >= 0 && row < ROWS && col >= 0 && col < COLS) g->tile[row][col] = t;
}
static void fill_tiles(passport_game_t *g, int r0, int c0, int r1, int c1, uint8_t t) {
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++) set_tile(g, r, c, t);
}
static void add_pipe(passport_game_t *g, int col, int height) {
    if (g->pipe_count >= PASSPORT_GAME_PIPE_MAX) return;
    g->pipes[g->pipe_count].col = (int16_t)col;
    g->pipes[g->pipe_count].height = (uint8_t)height;
    g->pipe_count++;
    int top = 13 - height;
    set_tile(g, top, col, PASSPORT_TILE_PIPE_TL);
    set_tile(g, top, col + 1, PASSPORT_TILE_PIPE_TR);
    for (int r = top + 1; r <= 12; r++) {
        set_tile(g, r, col, PASSPORT_TILE_PIPE_BL);
        set_tile(g, r, col + 1, PASSPORT_TILE_PIPE_BR);
    }
}
/* Ascending stairs to the right: column col+i is i+1 blocks tall. */
static void stair_up(passport_game_t *g, int col, int height) {
    for (int i = 0; i < height; i++)
        fill_tiles(g, 12 - i, col + i, 12, col + i, PASSPORT_TILE_STAIR);
}
/* Descending stairs to the right: column col+i is height-i blocks tall. */
static void stair_down(passport_game_t *g, int col, int height) {
    for (int i = 0; i < height; i++)
        fill_tiles(g, 13 - (height - i), col + i, 12, col + i, PASSPORT_TILE_STAIR);
}

static void build_level(passport_game_t *g) {
    memset(g->tile, 0, sizeof g->tile);
    g->pipe_count = 0;
    for (unsigned s = 0; s < sizeof GROUND_SEGS / sizeof GROUND_SEGS[0]; s++)
        fill_tiles(g, 13, GROUND_SEGS[s][0], 14, GROUND_SEGS[s][1], PASSPORT_TILE_GROUND);
    for (unsigned p = 0; p < sizeof PIPES / sizeof PIPES[0]; p++)
        add_pipe(g, PIPES[p][0], PIPES[p][1]);
    /* Opening question block, then the classic brick/question row. */
    set_tile(g, 9, 16, PASSPORT_TILE_QCOIN);
    set_tile(g, 9, 20, PASSPORT_TILE_BRICK);
    set_tile(g, 9, 21, PASSPORT_TILE_QCOIN);
    set_tile(g, 9, 22, PASSPORT_TILE_BRICK);
    set_tile(g, 9, 23, PASSPORT_TILE_QMUSH);
    set_tile(g, 9, 24, PASSPORT_TILE_BRICK);
    set_tile(g, 5, 22, PASSPORT_TILE_QCOIN);
    /* After the first pit. */
    set_tile(g, 9, 77, PASSPORT_TILE_BRICK);
    set_tile(g, 9, 78, PASSPORT_TILE_BRICK);
    set_tile(g, 9, 79, PASSPORT_TILE_BRICK);
    set_tile(g, 9, 91, PASSPORT_TILE_BRICK);
    set_tile(g, 9, 92, PASSPORT_TILE_QCOIN);
    set_tile(g, 9, 93, PASSPORT_TILE_BRICK);
    /* Second mushroom and the high block above it. */
    set_tile(g, 9, 106, PASSPORT_TILE_QCOIN);
    set_tile(g, 9, 109, PASSPORT_TILE_QMUSH);
    set_tile(g, 9, 112, PASSPORT_TILE_QCOIN);
    set_tile(g, 5, 112, PASSPORT_TILE_QCOIN);
    /* Twin pyramids around the wide pit. */
    stair_up(g, 118, 4);
    stair_down(g, 122, 4);
    stair_up(g, 131, 4);
    stair_down(g, 139, 4);
    /* Brick/question alternation before the final approach. */
    for (int c = 148; c <= 155; c++)
        set_tile(g, 9, c, (c & 1) ? PASSPORT_TILE_QCOIN : PASSPORT_TILE_BRICK);
    set_tile(g, 5, 170, PASSPORT_TILE_BRICK);
    set_tile(g, 5, 171, PASSPORT_TILE_BRICK);
    set_tile(g, 5, 172, PASSPORT_TILE_BRICK);
    set_tile(g, 5, 173, PASSPORT_TILE_BRICK);
    /* Final staircase, flagpole base and the level end wall. */
    stair_up(g, 181, 8);
    set_tile(g, 12, FLAG_COL_POS, PASSPORT_TILE_STAIR);
    fill_tiles(g, 5, 209, 14, 211, PASSPORT_TILE_SOLID);
    g->flag_col = FLAG_COL_POS;
    g->castle_col = CASTLE_COL_POS;
}

static int player_h(const passport_game_t *g) {
    return g->big ? PLAYER_H_BIG : PLAYER_H_SMALL;
}
static uint8_t tile_at(const passport_game_t *g, int row, int col) {
    if (col < 0 || col >= COLS) return PASSPORT_TILE_SOLID; /* level bounds */
    if (row < 0 || row >= ROWS) return PASSPORT_TILE_EMPTY; /* open sky/pit */
    return g->tile[row][col];
}
static bool tile_solid(uint8_t t) {
    return t == PASSPORT_TILE_GROUND || t == PASSPORT_TILE_BRICK ||
           t == PASSPORT_TILE_QCOIN || t == PASSPORT_TILE_QMUSH ||
           t == PASSPORT_TILE_USED || t == PASSPORT_TILE_STAIR ||
           t >= PASSPORT_TILE_PIPE_TL;
}
static bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}
static int surface_row(const passport_game_t *g, int col) {
    for (int r = 0; r < ROWS; r++)
        if (tile_solid(tile_at(g, r, col))) return r;
    return 13;
}
static uint16_t loop_time(const passport_game_t *g) {
    uint16_t t = TIME_START - 25 * (g->loop ? g->loop - 1 : 0);
    return t < 200 ? 200 : t;
}

/* ---- Block interaction from below. ---- */
static void spawn_fragments(passport_game_t *g, int row, int col) {
    static const int8_t vx[4] = {-50, -20, 20, 50};
    static const int16_t vy[4] = {-300, -260, -200, -160};
    for (int i = 0; i < PASSPORT_GAME_FRAGMENT_POOL; i++) {
        passport_game_fragment_t *f = &g->frag[i];
        f->active = true;
        f->ms = 500;
        f->x_q8 = Q(col * TILE + 2 + (i & 1) * 6);
        f->y_q8 = Q(row * TILE + 2 + (i >> 1) * 6);
        f->vx_q8 = Q(vx[i]);
        f->vy_q8 = Q(vy[i]);
    }
}
static void spawn_popcoin(passport_game_t *g, int row, int col) {
    for (int i = 0; i < PASSPORT_GAME_POPCOIN_POOL; i++) {
        passport_game_popcoin_t *p = &g->popcoin[i];
        if (p->active) continue;
        p->active = true;
        p->ms = 420;
        p->x_q8 = Q(col * TILE + 4);
        p->y_q8 = Q(row * TILE - 12);
        p->vy_q8 = Q(POPCOIN_VY);
        return;
    }
}
static void hit_block(passport_game_t *g, int row, int col) {
    uint8_t t = g->tile[row][col];
    if (t == PASSPORT_TILE_BRICK) {
        g->tile[row][col] = PASSPORT_TILE_EMPTY;
        g->score += 50;
        spawn_fragments(g, row, col);
    } else if (t == PASSPORT_TILE_QCOIN) {
        g->tile[row][col] = PASSPORT_TILE_USED;
        g->score += 200;
        g->coins++;
        spawn_popcoin(g, row, col);
    } else if (t == PASSPORT_TILE_QMUSH) {
        g->tile[row][col] = PASSPORT_TILE_USED;
        passport_game_mushroom_t *m = &g->mushroom;
        m->active = true;
        m->emerging = true;
        m->x_q8 = Q(col * TILE + 1);
        m->y_q8 = Q(row * TILE + 2);          /* starts inside the block */
        m->emerge_to_q8 = Q(row * TILE - 14); /* rests on top when out */
        m->vy_q8 = 0;
        m->dir = 1;
    }
    /* GROUND, STAIR, USED and pipes just stop the rise. */
}

/* ---- Player movement, one sub-step of at most ~8 px. ---- */
static uint16_t remaining_ms(uint16_t value, uint32_t dt) {
    return value > dt ? (uint16_t)(value - dt) : 0;
}
static void launch_jump(passport_game_t *g, bool air) {
    if (!air && g->move_grace_ms && !g->left_held && !g->right_held)
        g->pvx_q8 = g->move_grace_vx_q8;
    g->pvy_q8 = Q(JUMP_VY);
    g->grounded = false;
    g->air_jump_used = air;
    g->ground_grace_ms = g->jump_buffer_ms = g->move_grace_ms = 0;
}
static void player_substep(passport_game_t *g, uint32_t dt_ms) {
    const int pw = PLAYER_W, ph = player_h(g);
    const int32_t dt = (int32_t)dt_ms;
    g->jump_buffer_ms = remaining_ms(g->jump_buffer_ms, dt_ms);
    g->move_grace_ms = remaining_ms(g->move_grace_ms, dt_ms);
    if (!g->grounded) g->ground_grace_ms = remaining_ms(g->ground_grace_ms, dt_ms);
    int dir = g->right_held ? 1 : g->left_held ? -1 : 0;
    if (dir != 0) {
        g->facing = (int8_t)dir;
        bool reversing = (g->pvx_q8 > 0 && dir < 0) || (g->pvx_q8 < 0 && dir > 0);
        int32_t accel = g->grounded ? (reversing ? ACCEL_TURN : ACCEL_GROUND) : ACCEL_AIR;
        g->pvx_q8 += dir * Q(accel) * dt / 1000;
        if (g->pvx_q8 > Q(WALK_MAX)) g->pvx_q8 = Q(WALK_MAX);
        if (g->pvx_q8 < -Q(WALK_MAX)) g->pvx_q8 = -Q(WALK_MAX);
    } else if (g->grounded) {
        int32_t drop = Q(FRICTION) * dt / 1000;
        if (g->pvx_q8 > drop) g->pvx_q8 -= drop;
        else if (g->pvx_q8 < -drop) g->pvx_q8 += drop;
        else g->pvx_q8 = 0;
    } /* Airborne without input: momentum persists (one-key ladder). */

    int y_now = g->py_q8 >> 8;
    int r0 = y_now / TILE, r1 = (y_now + ph - 1) / TILE;
    if (g->pvx_q8 != 0) {
        int32_t next_x_q8 = g->px_q8 + g->pvx_q8 * dt / 1000;
        int x = next_x_q8 >> 8;
        int lead = g->pvx_q8 > 0 ? x + pw - 1 : x;
        int col = lead / TILE;
        bool hit = false;
        for (int r = r0; r <= r1; r++)
            if (tile_solid(tile_at(g, r, col))) { hit = true; break; }
        if (hit) {
            x = g->pvx_q8 > 0 ? col * TILE - pw : (col + 1) * TILE;
            next_x_q8 = Q(x);
            g->pvx_q8 = 0;
        }
        if (next_x_q8 < g->cam_q8) {
            next_x_q8 = g->cam_q8;
            if (g->pvx_q8 < 0) g->pvx_q8 = 0;
        }
        g->px_q8 = next_x_q8;
        y_now = g->py_q8 >> 8;
        r0 = y_now / TILE;
        r1 = (y_now + ph - 1) / TILE;
    }

    int32_t grav = GRAVITY_FALL;
    if (g->pvy_q8 < 0) grav = GRAVITY_RISE;
    g->pvy_q8 += Q(grav) * dt / 1000;
    if (g->pvy_q8 > Q(TERMINAL)) g->pvy_q8 = Q(TERMINAL);

    int x = g->px_q8 >> 8;
    int prev_bottom = (g->py_q8 >> 8) + ph;
    g->py_q8 += g->pvy_q8 * dt / 1000;
    int y = g->py_q8 >> 8;
    g->grounded = false;
    if (g->pvy_q8 >= 0) {
        int bottom = y + ph;
        int row = bottom / TILE;
        if (prev_bottom <= row * TILE) {
            int c0 = x / TILE, c1 = (x + pw - 1) / TILE;
            for (int c = c0; c <= c1; c++) {
                if (tile_solid(tile_at(g, row, c))) {
                    g->py_q8 = Q(row * TILE - ph);
                    g->pvy_q8 = 0;
                    g->grounded = true;
                    g->air_jump_used = false; /* landing re-arms the double jump */
                    g->ground_grace_ms = GROUND_GRACE_MS;
                    if (g->jump_buffer_ms) launch_jump(g, false);
                    break;
                }
            }
        }
    } else {
        int row = y / TILE;
        int c0 = x / TILE, c1 = (x + pw - 1) / TILE;
        int best = -1, bestd = 1 << 30, center = x + pw / 2;
        for (int c = c0; c <= c1; c++) {
            if (!tile_solid(tile_at(g, row, c))) continue;
            int d = c * TILE + 8 - center;
            if (d < 0) d = -d;
            if (d < bestd) { bestd = d; best = c; }
        }
        if (best >= 0) {
            g->py_q8 = Q((row + 1) * TILE);
            g->pvy_q8 = 0;
            hit_block(g, row, best);
        }
    }
}

/* ---- Entities. ---- */
static void spawn_window(passport_game_t *g) {
    int limit = (g->cam_q8 >> 8) + PASSPORT_GAME_VIEW_W + 48;
    while (g->goomba_spawn_idx < sizeof GOOMBA_SPAWNS / sizeof GOOMBA_SPAWNS[0] &&
           GOOMBA_SPAWNS[g->goomba_spawn_idx] * TILE < limit) {
        int slot = -1;
        for (int i = 0; i < PASSPORT_GAME_GOOMBA_POOL; i++)
            if (!g->goombas[i].active) { slot = i; break; }
        if (slot < 0) break; /* pool busy; retry next step */
        int col = GOOMBA_SPAWNS[g->goomba_spawn_idx++];
        passport_game_goomba_t *e = &g->goombas[slot];
        e->active = true;
        e->squash_ms = 0;
        e->dir = -1;
        e->x_q8 = Q(col * TILE + 1);
        e->y_q8 = Q(surface_row(g, col) * TILE - 14);
        e->vy_q8 = 0;
    }
    while (g->coin_spawn_idx < sizeof COIN_SPAWNS / sizeof COIN_SPAWNS[0] &&
           COIN_SPAWNS[g->coin_spawn_idx][0] * TILE < limit) {
        int slot = -1;
        for (int i = 0; i < PASSPORT_GAME_COIN_POOL; i++)
            if (!g->coin[i].active) { slot = i; break; }
        if (slot < 0) break;
        const uint8_t *s = COIN_SPAWNS[g->coin_spawn_idx++];
        g->coin[slot].active = true;
        g->coin[slot].x_q8 = Q(s[0] * TILE + 2);
        g->coin[slot].y = s[1] * TILE + 2;
    }
}
/* Shared falling body for goombas and the mushroom: snaps onto the highest
 * surface crossed this step (row scan keeps fast falls tunnel-free). */
static bool entity_fall(passport_game_t *g, int32_t *x_q8, int32_t *y_q8,
                        int32_t *vy_q8, int w, int h, uint32_t dt_ms) {
    const int32_t dt = (int32_t)dt_ms;
    *vy_q8 += Q(GRAVITY_FALL) * dt / 1000;
    if (*vy_q8 > Q(TERMINAL)) *vy_q8 = Q(TERMINAL);
    int x = *x_q8 >> 8, prev_bottom = (*y_q8 >> 8) + h;
    *y_q8 += *vy_q8 * dt / 1000;
    int bottom = (*y_q8 >> 8) + h;
    int row_from = prev_bottom / TILE, row_to = bottom / TILE;
    for (int row = row_from; row <= row_to; row++) {
        if (prev_bottom > row * TILE) continue; /* already below this surface */
        int c0 = x / TILE, c1 = (x + w - 1) / TILE;
        for (int c = c0; c <= c1; c++) {
            if (tile_solid(tile_at(g, row, c))) {
                *y_q8 = Q(row * TILE - h);
                *vy_q8 = 0;
                return true; /* landed */
            }
        }
    }
    return false;
}
static int goomba_speed(const passport_game_t *g) {
    int v = GOOMBA_SPEED_BASE + GOOMBA_SPEED_PER_LOOP * g->loop;
    return v > GOOMBA_SPEED_MAX ? GOOMBA_SPEED_MAX : v;
}
static void goomba_step(passport_game_t *g, int i, uint32_t dt_ms) {
    passport_game_goomba_t *e = &g->goombas[i];
    const int32_t dt = (int32_t)dt_ms;
    if (e->squash_ms) {
        e->squash_ms = e->squash_ms > dt_ms ? (uint16_t)(e->squash_ms - dt_ms) : 0;
        if (!e->squash_ms) e->active = false;
        return;
    }
    if (e->x_q8 < g->cam_q8 - Q(48)) { e->active = false; return; }
    e->x_q8 += e->dir * Q(goomba_speed(g)) * dt / 1000;
    int x = e->x_q8 >> 8, y = e->y_q8 >> 8;
    int lead = e->dir > 0 ? x + 13 : x;
    int col = lead / TILE;
    for (int r = y / TILE; r <= (y + 13) / TILE; r++) {
        if (tile_solid(tile_at(g, r, col))) {
            e->x_q8 = e->dir > 0 ? Q(col * TILE - 14) : Q((col + 1) * TILE);
            e->dir = (int8_t)-e->dir;
            break;
        }
    }
    entity_fall(g, &e->x_q8, &e->y_q8, &e->vy_q8, 14, 14, dt_ms);
    if ((e->y_q8 >> 8) > 280) e->active = false; /* pit */
}
static void mushroom_step(passport_game_t *g, uint32_t dt_ms) {
    passport_game_mushroom_t *m = &g->mushroom;
    const int32_t dt = (int32_t)dt_ms;
    if (!m->active) return;
    if (m->emerging) {
        m->y_q8 -= Q(MUSHROOM_RISE) * dt / 1000;
        if (m->y_q8 <= m->emerge_to_q8) { m->y_q8 = m->emerge_to_q8; m->emerging = false; }
        return;
    }
    m->x_q8 += m->dir * Q(MUSHROOM_SPEED) * dt / 1000;
    int x = m->x_q8 >> 8, y = m->y_q8 >> 8;
    int lead = m->dir > 0 ? x + 13 : x;
    int col = lead / TILE;
    for (int r = y / TILE; r <= (y + 13) / TILE; r++) {
        if (tile_solid(tile_at(g, r, col))) {
            m->x_q8 = m->dir > 0 ? Q(col * TILE - 14) : Q((col + 1) * TILE);
            m->dir = (int8_t)-m->dir;
            break;
        }
    }
    entity_fall(g, &m->x_q8, &m->y_q8, &m->vy_q8, 14, 14, dt_ms);
    if ((m->y_q8 >> 8) > 280) m->active = false;
}
static void effects_step(passport_game_t *g, uint32_t dt_ms) {
    for (int i = 0; i < PASSPORT_GAME_POPCOIN_POOL; i++) {
        passport_game_popcoin_t *p = &g->popcoin[i];
        if (!p->active) continue;
        p->ms = p->ms > dt_ms ? (uint16_t)(p->ms - dt_ms) : 0;
        if (!p->ms) { p->active = false; continue; }
        p->y_q8 += p->vy_q8 * (int32_t)dt_ms / 1000;
        p->vy_q8 += Q(POPCOIN_GRAV) * (int32_t)dt_ms / 1000;
    }
    for (int i = 0; i < PASSPORT_GAME_FRAGMENT_POOL; i++) {
        passport_game_fragment_t *f = &g->frag[i];
        if (!f->active) continue;
        f->ms = f->ms > dt_ms ? (uint16_t)(f->ms - dt_ms) : 0;
        if (!f->ms) { f->active = false; continue; }
        f->x_q8 += f->vx_q8 * (int32_t)dt_ms / 1000;
        f->y_q8 += f->vy_q8 * (int32_t)dt_ms / 1000;
        f->vy_q8 += Q(FRAG_GRAV) * (int32_t)dt_ms / 1000;
    }
}

/* ---- Player-versus-world interactions. ---- */
static void die(passport_game_t *g) {
    g->state = PASSPORT_GAME_DYING;
    g->death_ms = 0;
    g->big = false;
    g->pvy_q8 = Q(DEATH_VY);
    g->pvx_q8 = 0;
    g->grounded = false;
    g->ground_grace_ms = g->jump_buffer_ms = g->move_grace_ms = 0;
}
static void interactions(passport_game_t *g, bool falling_entry) {
    const int pw = PLAYER_W, ph = player_h(g);
    int px = g->px_q8 >> 8, py = g->py_q8 >> 8;
    /* Static coins. */
    for (int i = 0; i < PASSPORT_GAME_COIN_POOL; i++) {
        passport_game_coin_t *c = &g->coin[i];
        if (!c->active) continue;
        if (c->x_q8 < g->cam_q8 - Q(48)) { c->active = false; continue; }
        if (overlap(px + 1, py + 1, pw - 2, ph - 2, (c->x_q8 >> 8) + 1, c->y + 1, 10, 10)) {
            c->active = false;
            g->score += 200;
            g->coins++;
        }
    }
    /* Goombas. */
    for (int i = 0; i < PASSPORT_GAME_GOOMBA_POOL; i++) {
        passport_game_goomba_t *e = &g->goombas[i];
        if (!e->active || e->squash_ms) continue;
        int gx = e->x_q8 >> 8, gy = e->y_q8 >> 8;
        if (!overlap(px + 1, py + 1, pw - 2, ph - 2, gx + 1, gy + 1, 12, 12)) continue;
        /* Falling onto the top half squashes it; anything else hurts. The
         * entry flag keeps a stomp recognizable even when the same step also
         * lands the player on the floor next to the goomba. */
        bool stomp = falling_entry && py < gy + 8;
        if (stomp) {
            e->squash_ms = 350;
            g->score += 100;
            g->pvy_q8 = Q(STOMP_BOUNCE);
            g->grounded = false;
            g->air_jump_used = false; /* the bounce is a fresh launch */
            g->ground_grace_ms = g->jump_buffer_ms = g->move_grace_ms = 0;
        } else if (g->invuln_ms == 0) {
            if (g->big) {
                g->big = false;
                g->invuln_ms = 2000;
                g->py_q8 += Q(12); /* keep the feet planted while shrinking */
            } else {
                die(g);
                return;
            }
        }
    }
    /* Mushroom. */
    passport_game_mushroom_t *m = &g->mushroom;
    if (m->active && !m->emerging) {
        int mx = m->x_q8 >> 8, my = m->y_q8 >> 8;
        if (overlap(px + 1, py + 1, pw - 2, ph - 2, mx + 1, my + 1, 12, 12)) {
            m->active = false;
            g->score += 1000;
            if (!g->big) {
                g->big = true;
                g->py_q8 -= Q(12); /* grow upward; landing snaps the feet */
            }
        }
    }
}
static void flag_check(passport_game_t *g) {
    /* The pole rises from the base block at flag_col, so its reachable face
     * is that column's left edge (the player hugs the block to grab it). */
    int pole_x = g->flag_col * TILE;
    if ((g->px_q8 >> 8) + PLAYER_W < pole_x) return;
    g->state = PASSPORT_GAME_CLEAR;
    g->clear_phase = PASSPORT_CLEAR_SLIDE;
    g->px_q8 = Q(pole_x - PLAYER_W + 2);
    g->pvx_q8 = 0;
    g->pvy_q8 = 0;
    g->facing = 1;
    g->flag_y_q8 = Q(POLE_TOP_Y);
    int height = (POLE_BASE_Y - (g->py_q8 >> 8)) / TILE;
    if (height < 1) height = 1;
    if (height > 9) height = 9;
    g->score += (uint32_t)height * 100;
}

/* ---- Level lifecycle. ---- */
static void reset_entities(passport_game_t *g) {
    memset(g->goombas, 0, sizeof g->goombas);
    memset(g->coin, 0, sizeof g->coin);
    memset(g->popcoin, 0, sizeof g->popcoin);
    memset(g->frag, 0, sizeof g->frag);
    memset(&g->mushroom, 0, sizeof g->mushroom);
    g->goomba_spawn_idx = 0;
    g->coin_spawn_idx = 0;
}
static void place_player(passport_game_t *g) {
    g->px_q8 = Q(2 * TILE);
    g->py_q8 = Q(13 * TILE - player_h(g));
    g->pvx_q8 = 0;
    g->pvy_q8 = 0;
    g->facing = 1;
    g->grounded = true;
    g->air_jump_used = false;
    g->ground_grace_ms = GROUND_GRACE_MS;
    g->jump_buffer_ms = g->move_grace_ms = 0;
    g->move_grace_vx_q8 = 0;
    g->cam_q8 = 0;
}
static void reset_level(passport_game_t *g, bool keep_power) {
    bool big = keep_power && g->big;
    build_level(g);
    reset_entities(g);
    g->big = big;
    g->invuln_ms = 0;
    place_player(g);
    spawn_window(g); /* the READY frame already shows nearby enemies and coins */
}

/* ---- Public API. ---- */
void passport_game_init(passport_game_t *g, uint32_t seed) {
    uint32_t best = g->best;
    memset(g, 0, sizeof *g);
    g->best = best;
    g->rng = seed;
    g->state = PASSPORT_GAME_READY;
    g->lives = 3;
    g->time = TIME_START;
    g->loop = 1;
    build_level(g);
    reset_entities(g);
    place_player(g);
    spawn_window(g); /* the READY frame already shows nearby enemies and coins */
}
static void direction_input(passport_game_t *g, bool *key, bool held) {
    if (*key && !held && g->grounded) {
        g->move_grace_vx_q8 = g->pvx_q8;
        g->move_grace_ms = MOVE_GRACE_MS;
    }
    if (held) g->move_grace_ms = 0; /* current steering overrides a handoff */
    *key = held;
}
void passport_game_left(passport_game_t *g, bool held) {
    direction_input(g, &g->left_held, held);
}
void passport_game_right(passport_game_t *g, bool held) {
    direction_input(g, &g->right_held, held);
}
void passport_game_reset_input(passport_game_t *g) {
    if (!g) return;
    g->left_held = g->right_held = false;
    g->pvx_q8 = g->move_grace_vx_q8 = 0;
    g->ground_grace_ms = g->jump_buffer_ms = g->move_grace_ms = 0;
}
void passport_game_jump(passport_game_t *g, bool down) {
    if (!down) return; /* fixed-height jump: releasing OK never cuts the arc */
    if (g->state == PASSPORT_GAME_READY) {
        g->state = PASSPORT_GAME_PLAYING;
        return;
    }
    if (g->state == PASSPORT_GAME_OVER) {
        passport_game_init(g, 0); /* full restart; best survives */
        return;
    }
    if (g->state == PASSPORT_GAME_CLEAR && g->clear_phase == PASSPORT_CLEAR_POSE) {
        g->loop++;
        g->time = loop_time(g);
        reset_level(g, true);
        g->state = PASSPORT_GAME_PLAYING;
        return;
    }
    if (g->state != PASSPORT_GAME_PLAYING) return;
    if (g->grounded || g->ground_grace_ms) {
        launch_jump(g, false);
    } else if (!g->air_jump_used) {
        launch_jump(g, true); /* one extra mid-air jump */
    } else g->jump_buffer_ms = JUMP_BUFFER_MS;
}
void passport_game_step(passport_game_t *g, uint32_t dt_ms) {
    if (!g) return;
    if (dt_ms > 60) dt_ms = 60;
    if (!dt_ms) return;
    if (g->state == PASSPORT_GAME_READY || g->state == PASSPORT_GAME_OVER) return;

    if (g->state == PASSPORT_GAME_DYING) {
        g->death_ms += dt_ms;
        g->pvy_q8 += Q(GRAVITY_FALL) * (int32_t)dt_ms / 1000;
        g->py_q8 += g->pvy_q8 * (int32_t)dt_ms / 1000;
        if (g->death_ms >= DEATH_MS) {
            if (g->lives <= 1) {
                g->lives = 0;
                g->state = PASSPORT_GAME_OVER;
                if (g->score > g->best) g->best = g->score;
            } else {
                g->lives--;
                g->time = loop_time(g);
                reset_level(g, false);
                g->state = PASSPORT_GAME_PLAYING;
            }
        }
        return;
    }

    if (g->state == PASSPORT_GAME_PLAYING) {
        g->time_acc_ms += dt_ms;
        while (g->time_acc_ms >= TIME_UNIT_MS) {
            g->time_acc_ms -= TIME_UNIT_MS;
            if (g->time > 0) g->time--;
            if (g->time == 0) { die(g); return; }
        }
        if (g->invuln_ms)
            g->invuln_ms = g->invuln_ms > dt_ms ? (uint16_t)(g->invuln_ms - dt_ms) : 0;
    }

    bool falling_entry = false;
    if (g->state == PASSPORT_GAME_CLEAR) {
        if (g->clear_phase == PASSPORT_CLEAR_SLIDE) {
            int floor_y = POLE_BASE_Y - player_h(g);
            g->py_q8 += Q(CLEAR_SLIDE_VY) * (int32_t)dt_ms / 1000;
            g->flag_y_q8 += Q(FLAG_SLIDE_VY) * (int32_t)dt_ms / 1000;
            bool p_done = g->py_q8 >= Q(floor_y);
            bool f_done = g->flag_y_q8 >= Q(FLAG_REST_Y);
            if (p_done) g->py_q8 = Q(floor_y);
            if (f_done) g->flag_y_q8 = Q(FLAG_REST_Y);
            if (p_done && f_done) g->clear_phase = PASSPORT_CLEAR_WALK;
        } else if (g->clear_phase == PASSPORT_CLEAR_WALK) {
            int32_t door = Q(g->castle_col * TILE + 18);
            g->px_q8 += Q(CLEAR_WALK_VX) * (int32_t)dt_ms / 1000;
            if (g->px_q8 >= door) {
                g->px_q8 = door;
                g->clear_phase = PASSPORT_CLEAR_POSE;
                g->clear_ms = 0;
                g->score += (uint32_t)g->time * 10;
                g->time = 0;
                if (g->score > g->best) g->best = g->score;
            }
        } else {
            g->clear_ms += dt_ms;
        }
    } else {
        /* Sub-step the player so no move exceeds ~8 px (tunnel safety). */
        falling_entry = g->pvy_q8 > 0;
        int32_t vmax = g->pvx_q8 < 0 ? -g->pvx_q8 : g->pvx_q8;
        int32_t vymax = g->pvy_q8 < 0 ? -g->pvy_q8 : g->pvy_q8;
        if (vymax > vmax) vmax = vymax;
        unsigned steps = 1 + (unsigned)((vmax >> 8) * (int32_t)dt_ms / 8000);
        if (steps > 8) steps = 8;
        uint32_t sub = dt_ms / steps, rest = dt_ms % steps;
        for (unsigned i = 0; i < steps; i++)
            player_substep(g, i ? sub : sub + rest);
        /* Camera follows right only, classic style. */
        int32_t target = g->px_q8 - Q(CAMERA_ANCHOR);
        int32_t maxc = Q(PASSPORT_GAME_LEVEL_W - PASSPORT_GAME_VIEW_W);
        if (target > maxc) target = maxc;
        if (target < 0) target = 0;
        if (target > g->cam_q8) g->cam_q8 = target;
        if ((g->py_q8 >> 8) > 300) { die(g); return; } /* pit */
        flag_check(g);
    }

    spawn_window(g);
    for (int i = 0; i < PASSPORT_GAME_GOOMBA_POOL; i++)
        if (g->goombas[i].active) goomba_step(g, i, dt_ms);
    mushroom_step(g, dt_ms);
    effects_step(g, dt_ms);
    if (g->state == PASSPORT_GAME_PLAYING) interactions(g, falling_entry);
}
