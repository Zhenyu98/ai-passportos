[简体中文](passport-game.zh_CN.md) · **English**

# Passport OS mini game

Date: 2026-10-23. Status: full side-scrolling platformer on `feature/passport-os-v0.1`, with collision and manual-control improvements; the original isolated branch `feature/passport-game` is history.

## Purpose

A Super-Mario-Bros-style side-scrolling platformer as a menu entry on the Passport OS badge. Offline, single-player, three buttons, no Wi-Fi/USB/audio dependency. The design references the classic 1-1 level shape — question blocks, brick rows, pipes, pits, twin stair pyramids, a flagpole and a castle — with an original layout, art and tuning.

## Landscape orientation and controls

The 240x320 panel is held sideways (rotated a quarter turn counterclockwise) so the game viewport is the familiar 320x240 landscape. Every game-space rectangle `(gx, gy, gw, gh)` is drawn at screen `(239 - gy - gh, gx)` with swapped extents, and text labels rotate 90 degrees around their centre to stay readable in that grip.

| Button | In game |
| --- | --- |
| UP | Walk left |
| DOWN | Walk right |
| OK click | Jump (a fixed full-height arc); also confirms the ready/clear/game-over screens |
| OK click again, mid-air | One extra double jump |
| OK hold, 2 s | Return to the badge (the global shortcut) |

### Shared ADC keys and forgiving jumps

The buttons share an ADC resistor ladder. UP shorts the node, masking OK and DOWN. DOWN+OK has a distinct nominal voltage (about 212 mV versus DOWN's 300 mV) and is decoded as both keys only while the game owns input. The initial configurable window is 170–250 mV; these values follow the circuit definition and still require actual-board measurement. Other pages retain single-key decoding.

- Hold DOWN and press OK to jump right, or press both together. UP+OK cannot be distinguished electrically: for a left jump, release UP and quickly press OK. For 120 ms after releasing a direction, jumping preserves the release velocity; pressing another direction cancels that handoff.
- An OK press commits a full-height jump; releasing OK does nothing. Walking off a ledge leaves a 100 ms ground-jump grace period, preserving the double jump.
- Air momentum persists and air steering stays on: releasing the direction key mid-jump does not slow the player, and pressing a direction mid-flight only steers, it never shortens the arc.
- One extra jump is available mid-air (double jump), re-armed on every landing and every stomp bounce, so the tall pipes and the flag-top bonus stay reachable.
- After the double jump is spent, a jump press can wait up to 120 ms for landing and then launch once. Holding OK does not automatically repeat jumps. Lost input clears held directions and pending actions, and waits for every key to be released.

## Mechanics

- Walk with acceleration, skid-turning and ground friction; fixed walk speed cap. Camera follows right only and never scrolls back; the view's left edge walls the player in, classic style.
- Head-bump question blocks for coins (+200, coin counter) or a mushroom (+1000); the mushroom emerges from the block, walks, and grows the player. Big player takes one hit (shrinks, 2 s invulnerable blink) instead of dying.
- Bricks break from below (+50, four flying shards). Static coins are collected on contact (+200).
- Goombas walk toward the player, turn at walls, and fall into pits. Falling goombas and mushrooms land on the first surface crossed, including delayed frames that cross both ground rows. Stomp from above (+100, squash animation, a bounce that also re-arms the double jump); side contact hurts.
- Three lives. Death pits, enemies and the countdown timer all kill; time ticks every 400 ms and each cleared loop starts the next round with less time and faster goombas.
- Grab the flagpole (height bonus 100 x tiles), watch the slide-and-walk-to-the-castle sequence, bank the remaining time x10 as score, then OK starts the next loop with the power-up and score kept.
- Best score is kept in RAM only and survives restarts until power-off; persistence is intentionally out of scope.

## Implementation

- `main/passport_game.c` / `main/passport_game.h`: pure, deterministic Q8 fixed-point simulation — no FPU, no LVGL, no ESP-IDF. Authored 212-column level from segment/pipe/spawn tables, tile-based collision with sub-stepped movement (no tunnelling), head-bump block interaction, goomba/mushroom/popcoin/fragment entities, camera, lives and loop state machine. Covered by `tests/test_passport_game.c` including a bit-for-bit determinism check.
- `main/passport_ui.c`: LVGL rendering in the rotated frame. The entire playfield is painted by one object's `LV_EVENT_DRAW_MAIN` handler issuing `lv_draw_rect` calls — tiles as per-row run-length fills, pipes/flag/castle/coins/goombas/mushroom/fragments and the five-part player sprite all resolve to world-space rectangles mapped to the portrait panel with a quarter-turn transform. On top of the painter sit only ten widgets: four rotated HUD labels, the overlay panel and four rotated overlay labels, all fed from per-field caches. The renderer reads the game's view and never mutates game state. The widget count stays this low because the shared 32 KB LVGL pool (now 64 KB, see below) also has to absorb every other page.
- Rotated text goes through LVGL's transform layers: each rotated label renders into a temporary ARGB8888 buffer of roughly `length x height x 4` bytes allocated from the LVGL pool. With the game page live, those peaks exceeded what a 32 KB pool could guarantee, and LVGL 9.5's single-threaded SW draw unit retries a failed layer allocation forever (a busy hang, not a crash). `sdkconfig.defaults` therefore raises `CONFIG_LV_MEM_SIZE_KILOBYTES` from 32 to 64 — the ESP32-C3 still keeps roughly 90 KB of free heap after boot — and `tools/preview/CMakeLists.txt` mirrors the value so the host preview exercises the same budget. Labels lay text out along their long axis first and rotate about their centre, so a landscape label of length `gw` and line height `gh` is a plain `gw x gh` label; the rotated rect's centre and the unrotated label's centre coincide.
- `main/passport_model.c` / `main/passport_app.c`: GAME forwards raw edges to `passport_ui_game_key`, opts into BSP DOWN+OK recognition, and clears inputs on ownership loss. The model tracks only OK in the game so direction events cannot disrupt the long-OK home shortcut.
- `assets/passport/ui-text.json` plus the regenerated `passport_strings.h` and `manifest.json` add the Chinese HUD/overlay strings; all glyphs are already covered by the GB2312 font subset.

## Validation

Follow the repository gate: `./tools/validate.sh --static` (repository checks, host tests, and the real LVGL preview) and `./tools/validate.sh --firmware` (ESP-IDF 5.5.3 build and merged-image verification). Host regressions cover natural enemy falls at 51/52/60 ms, mushroom landing, fractional steering, jump timing, input recovery and the actual BSP decoder with simulated voltages. Physical combination-voltage stability, sideways-grip controls and display rendering remain hardware acceptance checks.
