#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Super-Mario-Bros-style side-scrolling platformer, pure logic: no LVGL, no
 * ESP-IDF. The level is a 16-px tile grid rendered through a 320x240 landscape
 * viewport; the UI rotates that view onto the portrait 240x320 panel (UP key
 * moves left, DOWN moves right, OK jumps - see docs/assets/passport-game.md).
 * All positions and velocities use Q8 fixed point so behaviour is
 * deterministic on the host and on the ESP32-C3 (no FPU).
 *
 * Keys share one ADC resistor ladder. DOWN+OK can be decoded together when the
 * BSP's opt-in combination mode is enabled; UP masks every other key. Fixed
 * jumps, air momentum, a short release-to-jump handoff, ground grace and jump
 * buffering keep manual movement usable on that asymmetric hardware. */

#define PASSPORT_GAME_TILE 16
#define PASSPORT_GAME_VIEW_W 320
#define PASSPORT_GAME_VIEW_H 240
#define PASSPORT_GAME_ROWS 15
#define PASSPORT_GAME_COLS 212
#define PASSPORT_GAME_LEVEL_W (PASSPORT_GAME_COLS * PASSPORT_GAME_TILE)

/* Entity pools: the renderer sizes its LVGL object pools to these. */
#define PASSPORT_GAME_GOOMBA_POOL 3
#define PASSPORT_GAME_COIN_POOL 8
#define PASSPORT_GAME_PIPE_MAX 6
#define PASSPORT_GAME_POPCOIN_POOL 2
#define PASSPORT_GAME_FRAGMENT_POOL 4

/* Tile codes stored in passport_game_t::tile (runtime-mutable: bricks break,
 * question blocks turn into used blocks). */
enum {
    PASSPORT_TILE_EMPTY = 0,
    PASSPORT_TILE_GROUND,   /* soil, rows 13-14, with pits */
    PASSPORT_TILE_BRICK,    /* breakable from below */
    PASSPORT_TILE_QCOIN,    /* question block: coin */
    PASSPORT_TILE_QMUSH,    /* question block: mushroom */
    PASSPORT_TILE_USED,     /* spent question block */
    PASSPORT_TILE_STAIR,    /* pyramid / staircase block */
    PASSPORT_TILE_PIPE_TL,  /* pipe top-left */
    PASSPORT_TILE_PIPE_TR,  /* pipe top-right */
    PASSPORT_TILE_PIPE_BL,  /* pipe body-left */
    PASSPORT_TILE_PIPE_BR,  /* pipe body-right */
    PASSPORT_TILE_SOLID     /* level end wall */
};

typedef enum {
    PASSPORT_GAME_READY,   /* title overlay; first OK press starts the run */
    PASSPORT_GAME_PLAYING,
    PASSPORT_GAME_DYING,   /* death arc, then respawn or game over */
    PASSPORT_GAME_CLEAR,   /* grabbed the flagpole: slide, walk, pose */
    PASSPORT_GAME_OVER
} passport_game_state_t;

/* Phases of PASSPORT_GAME_CLEAR. */
enum {
    PASSPORT_CLEAR_SLIDE,  /* riding the flagpole down */
    PASSPORT_CLEAR_WALK,   /* auto-walking to the castle door */
    PASSPORT_CLEAR_POSE    /* standing at the door; OK advances the loop */
};

typedef struct {
    int16_t col;     /* pipe left column, tiles */
    uint8_t height;  /* total height in tiles (top row = 13 - height) */
} passport_game_pipe_t;

typedef struct {
    bool active;
    int32_t x_q8, y_q8;   /* world position of the hitbox top-left, Q8 */
    int32_t vy_q8;        /* fall speed, Q8 (row-scan landing) */
    int8_t dir;           /* -1 left, +1 right */
    uint16_t squash_ms;   /* >0: flattened and fading */
} passport_game_goomba_t;

typedef struct {
    bool active;
    int32_t x_q8;   /* world position, Q8 */
    int16_t y;      /* top edge, view-local px */
} passport_game_coin_t;

typedef struct {
    bool active;
    int32_t x_q8, y_q8, vy_q8;  /* pops out of a block and arcs up, Q8 */
    uint16_t ms;
} passport_game_popcoin_t;

typedef struct {
    bool active;
    int32_t x_q8, y_q8, vx_q8, vy_q8;  /* brick shards, Q8 */
    uint16_t ms;
} passport_game_fragment_t;

typedef struct {
    bool active, emerging;  /* emerging: rising out of the block it came from */
    int32_t x_q8, y_q8, vy_q8;
    int32_t emerge_to_q8;   /* target top edge while emerging, Q8 */
    int8_t dir;
} passport_game_mushroom_t;

typedef struct {
    /* ---- Read-only view for the renderer. ---- */
    passport_game_state_t state;
    uint8_t clear_phase;
    uint32_t score, best;      /* best lives in RAM only */
    uint16_t coins;
    uint8_t lives;
    uint16_t time;             /* remaining time units (start 300) */
    uint8_t loop;              /* difficulty round after each clear */
    bool big;                  /* mushroom power-up */
    int8_t facing;             /* -1 left, +1 right */
    uint16_t invuln_ms;        /* blink window after shrinking */
    bool grounded;
    int32_t px_q8, py_q8;      /* player top-left, world coords, Q8 */
    int32_t pvx_q8, pvy_q8;    /* velocities, px/s in Q8 */
    int32_t cam_q8;            /* camera left edge, world coords, Q8 */
    uint8_t tile[PASSPORT_GAME_ROWS][PASSPORT_GAME_COLS];
    passport_game_pipe_t pipes[PASSPORT_GAME_PIPE_MAX];
    uint8_t pipe_count;
    passport_game_goomba_t goombas[PASSPORT_GAME_GOOMBA_POOL];
    passport_game_coin_t coin[PASSPORT_GAME_COIN_POOL];
    passport_game_popcoin_t popcoin[PASSPORT_GAME_POPCOIN_POOL];
    passport_game_fragment_t frag[PASSPORT_GAME_FRAGMENT_POOL];
    passport_game_mushroom_t mushroom;
    int16_t flag_col, castle_col;  /* tile columns; the pole and castle follow */
    int32_t flag_y_q8;             /* flag position on the pole, Q8 */
    /* ---- Internals; the renderer must not read these. ---- */
    uint32_t rng;
    uint32_t time_acc_ms;      /* accumulates 400 ms per time unit */
    uint16_t death_ms;         /* death arc stopwatch */
    uint16_t clear_ms;         /* pose stopwatch */
    bool air_jump_used, left_held, right_held;  /* air_jump_used: double jump spent */
    uint16_t ground_grace_ms, jump_buffer_ms, move_grace_ms;
    int32_t move_grace_vx_q8;
    unsigned goomba_spawn_idx, coin_spawn_idx;
} passport_game_t;

/* Reset to READY with a fresh world. The level is fixed (the seed only feeds
 * cosmetic randomness); best score is preserved across runs. */
void passport_game_init(passport_game_t *g, uint32_t seed);

/* Held levels for movement. UP/DOWN on the device map to left/right. */
void passport_game_left(passport_game_t *g, bool held);
void passport_game_right(passport_game_t *g, bool held);
/* Lost input/page exit: stop horizontal motion and discard queued actions.
 * Keeps vertical motion and the spent-air-jump state. */
void passport_game_reset_input(passport_game_t *g);

/* OK edge: press starts a READY run, jumps when grounded with a full-height
 * arc, performs one extra mid-air jump, advances CLEAR-pose to the next loop,
 * and restarts from OVER. Ground grace is 100 ms; a spent jump can wait 120 ms
 * for landing. Releasing OK is a no-op (jumps never cut). */
void passport_game_jump(passport_game_t *g, bool down);

/* Advance the simulation; dt is clamped to 60 ms per call. */
void passport_game_step(passport_game_t *g, uint32_t dt_ms);
