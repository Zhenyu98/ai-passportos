[简体中文](passport-os-v0.1-plan.zh_CN.md) · **English**

# Passport OS v0.1 implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for native execution, or superpowers:subagent-driven-development if the user selects that method. Steps use checkbox syntax for tracking.

**Goal:** Deliver the approved offline identity-card increment with local configuration, persistent data, a real QR code, and a small avatar animation.

**Architecture:** Pure C models own validation, navigation, record serialization, and QR layout. ESP-IDF adapters own NVS and USB; an application owner updates the redesigned LVGL pages. Bounded worker requests keep storage and parsing out of button callbacks and LVGL timers.

**Tech Stack:** ESP-IDF 5.5.3, C11, LVGL 9.5.0, existing BSP, cJSON from the SDK, Python 3.12 host tooling, Node.js with lv_font_conv 1.5.3.

**Spec:** [Approved design](passport-os-v0.1-design.md), approved by the user on 2026-10-20.

## Global constraints

- Baseline: `3d685a90bdb96dd8526d84edde1e63fec25b460e`; branch `feature/passport-os-v0.1`; ESP32-C3, 8 MB Flash, no PSRAM.
- Windows native build, no WSL migration or global configuration edits. Use a space-free short-path alias of the checkout and validate it before use.
- Preserve BSP pins, the USB console, partition data, unrelated files, and the dependency lock. Do not retain the baseline test UI as the product UI.
- Field byte limits excluding NUL: name 64, title 96, research 192, bio 512, homepage 128. Only HTTP/HTTPS homepages with a host are supported.
- NVS namespace `passport`, record schema 1; rejected updates and failed commits retain the previous profile/settings.
- Brightness 30/60/90%, default 60%; 2,000 ms OK hold returns to Badge; time displays in UTC+08:00 or remains explicitly unset.
- QR available square 196 pixels, four-module quiet zone, integer scale at least 3; avatar three 64-by-64 RGB565 frames, at most 24,576 pixel bytes.
- No active radio/audio stacks, XiaoZhi, MCP, Muse, transcription, or BLE editing in this increment. Voice notes remain pending the replacement transcription service.
- Private JSON, personal images, generated personal fonts, logs, and firmware artifacts remain ignored and local. No automatic flash, push, or publication.

## Review focus

1. OK release after the 2-second hold, duplicate BSP events, and queue overflow must never cause an extra click or leave navigation permanently stuck (Tasks 2 and 5).
2. Invalid/partial profile data, NVS corruption, and failed writes must not destroy the last saved record or unrelated namespaces (Task 2).
3. Escaped NULs, trailing JSON, duplicate keys, oversize lines, stale replies, and absent glyphs must fail without data changes or private log output (Task 3).
4. Short and maximum-length URLs must decode to the exact original bytes with a real four-module quiet zone, including after replacing an earlier QR (Task 4).
5. Repeated page changes and concurrent configuration must release page timers/buffers and keep the UI responsive when optional peripherals or allocations fail (Task 5).

## File map

| Area | Files and responsibility |
| --- | --- |
| Pure application | `main/passport_model.[ch]`, `passport_profile.[ch]`, `passport_store.[ch]`, `passport_qr.[ch]`: input/state, bounded text, stable records, QR matrix/layout. |
| Platform adapters | `main/passport_nvs.[ch]`, `passport_protocol.[ch]`, `passport_usb.[ch]`: NVS backend, bounded JSON messages, USB worker. |
| Presentation | `main/passport_ui.[ch]`, `passport_app.[ch]`, `passport_assets.h`, replacement `main/main.c`: pages, ownership/lifecycle, generated asset declarations, badge-first boot. |
| Host tools | `tools/test_passport.py`, `tools/configure_passport.py`, `tools/build_passport_assets.py`: focused test runner, USB utility, reproducible asset generation. `tools/preview_passport.c`, `tools/preview/CMakeLists.txt`, and `tools/render_passport_preview.py` render the actual UI with headless LVGL. |
| Assets/tests | `assets/passport/ui-text.json`, public fonts/default frames and license records; `tests/test_passport_*` for behavior and fault injection. |
| Integration | `main/CMakeLists.txt`, `sdkconfig.defaults`, `tools/validate.sh`, `.gitignore`, paired asset/user guides and indexes. |

## Task 1: Prepare native tools and preserve a baseline build

**Files:** Existing `tools/validate.sh`, `tools/install_passport_skills.py`, and engineering guides are inputs. Retain machine-specific setup manifests and logs under ignored `build/setup/` and `build/baseline/`; no application source changes yet.

**Interfaces:** Produce an activated PowerShell environment reporting `ESP-IDF v5.5.3`, the unchanged dependency lock, and a baseline firmware/debug bundle. Pass the environment to later tasks; do not hardcode machine paths in application code.

- [x] Inspect existing installations again; run the skill installer's read-only check. Install missing project skills using readable copies or supported links, preserving existing destinations, then verify all five discovery paths. Read passport-develop and passport-setup for their stages. Use environment approval for writes outside the workspace; do not equate reading canonical sources with successful installation.
- [x] Verify the short-path alias resolves to this checkout. Install ESP-IDF 5.5.3 side by side through the official native Windows setup into a space-free tools directory; use its Python environment. Keep this installation location under 90 characters and free of spaces. Reuse the bundled Python 3.12 for host tools where suitable, rather than changing the user's Python 3.14 installation.
- [x] Supply Git Bash with a verified native host C compiler, Python, and a verified Windows actionlint 1.7.12 binary through command-scoped `CC`, `PATH`, and `ACTIONLINT_BIN`. Prefer existing tools; if absent, use the official LLVM-MinGW distribution and verify its published checksum. Do not run the Linux-only actionlint installer fallback on Windows. Record exact tool versions/download hashes in the setup manifest.
- [x] In the activated environment, run the unchanged `bash ./tools/validate.sh --static`, then `bash ./tools/validate.sh --firmware` from the short project path. Retain complete exit codes/logs and the archived baseline bundle. The expected result is Host tests PASS and Firmware build PASS; diagnose any platform incompatibility before application changes, without weakening the gate.
- [x] Record baseline heap checks as hardware-pending, and preserve the exact commit and lock hash. A baseline build does not flash COM7 or prove hardware compatibility.

## Task 2: Implement navigation, validation, and transactional records

**Files:** Create `main/passport_model.[ch]`, `main/passport_profile.[ch]`, `main/passport_store.[ch]`, `main/passport_nvs.[ch]`, `tools/test_passport.py`, and `tests/test_passport_model.c`, `test_passport_profile.c`, `test_passport_store.c`.

**Interfaces:** `passport_profile_t` contains the five spec fields, each with capacity limit plus NUL. `passport_record_t` adds schema and brightness. Produce `bool passport_profile_validate(const passport_profile_t *)`; `void passport_model_init(passport_model_t *, uint8_t brightness)`; `passport_action_t passport_model_input(passport_model_t *, passport_key_t, passport_key_event_t, uint64_t now_ms)`; `passport_action_t passport_model_tick(passport_model_t *, uint64_t now_ms)`. Actions are `PASSPORT_ACTION_NONE`, `PASSPORT_ACTION_RENDER`, and `PASSPORT_ACTION_SAVE_BRIGHTNESS`. Produce `passport_status_t passport_store_load(const passport_store_backend_t *, passport_record_t *out)` and `passport_status_t passport_store_save(const passport_store_backend_t *, passport_record_t *current, const passport_record_t *candidate)`. The backend has `ctx`, a `read(ctx, uint8_t *buffer, size_t *inout_length)` callback, and a `write(ctx, const uint8_t *buffer, size_t length)` callback, both returning `passport_status_t`. The same record logic is tested with failures and used by the NVS adapter.

- [x] Write failing C tests: cold boot is Badge; page wrap and Profile boundary scrolling; menu origin restoration; all three brightness levels; OK held for 1,999 ms does not return, 2,000 ms returns exactly once, and release does not open Menu. Two press/release cycles give two clicks even when CLICK/DOUBLE/LONG notifications are ignored. A lost release cancels safely after 10 seconds without inventing a click.
- [x] Write failing validation/store tests: each byte-limit boundary, multibyte boundary, invalid UTF-8, empty fields, URL without a host, unsupported schemes, and controls. Allow newline only in research/bio. Verify schema/CRC errors, a truncated record, a failed backend commit, and successful replacement; failed saves preserve the previous snapshot and never invoke an erase callback.
- [x] Run `python tools/test_passport.py model profile store` and retain the initial missing/failed behavior. Implement the pure modules, stable explicitly serialized schema-1 record with length/CRC checks, and a single-key NVS adapter. Do not serialize raw C structure padding. `nvs_flash_init()` errors must not trigger a blanket erase.
- [x] Run the same command until all assertions pass under `-std=c11 -Wall -Wextra -Werror`. The runner accepts named groups and locates the native compiler from the environment; it creates output only under ignored `build/host-tests/`.
- [x] Review the diff and retain verification evidence. Keep commits local and scoped when the active workflow requires a checkpoint; never push automatically.

## Task 3: Implement bounded USB configuration

**Files:** Create `main/passport_protocol.[ch]`, `main/passport_usb.[ch]`, `tools/configure_passport.py`, `tests/test_passport_protocol.c`, and `tests/test_passport_configure.py`; extend the focused runner.

**Interfaces:** Consume Task 2 profiles/store validation. `passport_status_t passport_protocol_parse(const char *line, size_t length, passport_request_t *out)` parses without executing. `passport_request_t` carries a uint32 ID, operation, and typed payload. Operations are `info`, `get`, `set`, and `time`; requests are capped at 8,192 bytes. Responses begin `PASSPORT/1 ` and echo ID plus `ok`; info includes schema, capabilities, and glyph inventory. `typedef bool (*passport_glyph_fn)(uint32_t codepoint, void *user)` is supplied by Task 4 assets.

Define `passport_reply_t` as the typed result of those four operations. The platform service hook is `typedef passport_status_t (*passport_service_fn)(const passport_request_t *, passport_reply_t *, void *user)`. `esp_err_t passport_usb_start(passport_service_fn, void *user)` binds it; the hook runs in the worker, never in a button/LVGL callback. `passport_status_t passport_protocol_reply(const passport_reply_t *, char *out, size_t capacity, size_t *written)` serializes bounded replies. Task 5 supplies the service implementation and posts committed snapshots to the UI owner.

- [x] Write failing tests for valid requests, wrong types/operations, unknown/duplicate keys, embedded decoded NUL, trailing garbage, truncated/over-limit lines, missing glyphs, and epoch range. For `time`, accept UTC epoch seconds from 2020-01-01 through 2099-12-31. Rejected requests invoke no backend writes; errors identify a field/code without echoing its private value.
- [x] Write Python transport tests using a fake serial object: unrelated device logs, stale IDs, malformed prefixed responses, timeouts, and a rejected set do not count as success. Import validation and glyph checks happen before a set is sent. Require explicit `--port`; keep DTR/RTS disabled and do not auto-select Bluetooth ports.
- [x] Run `python tools/test_passport.py protocol` and `python -m unittest discover -s tests -p test_passport_configure.py` to establish failures. Implement parsing with the SDK's cJSON, raw-string escape checks for NUL, bounded line accumulation with discard-until-newline recovery, and the host commands `info`, `get`, `set --profile`, `time --now`.
- [x] Route parsed requests through one bounded work queue and immutable owned payloads; do not store pointers into reused receive buffers. The application receives committed snapshots only after successful persistence. USB parsing and NVS work stay outside LVGL/button callbacks.
- [x] Rerun both commands until PASS, including a fault-injected commit failure and an oversize line followed by a valid line. Serial transport is tested with fakes until the new firmware is authorized and installed.

## Task 4: Generate real QR matrices and reproducible assets

**Files:** Create `main/passport_qr.[ch]`, `main/passport_assets.h`, `tools/build_passport_assets.py`, `assets/passport/ui-text.json`, public default asset sources, `tests/test_passport_qr.c`, and `tests/test_passport_assets.py`. Update `assets/README.md` and its Chinese pair; keep private generated assets under ignored `build/passport-assets/`.

**Interfaces:** `bool passport_qr_encode(const char *url, passport_qr_t *out)` uses the pinned LVGL qrcodegen source and owns a fixed 512-byte matrix buffer. Export module count, integer scale, and centered origin; `bool passport_qr_module(const passport_qr_t *, int x, int y)` exposes matrix bits. `bool passport_assets_has_glyph(uint32_t codepoint, void *user)` matches Task 3 callback. Generate `passport_font_16`, `passport_font_20`, the glyph inventory, and three avatar descriptors with application lifetime.

- [x] Write failing tests: empty/invalid URL produces no QR; a public test URL and a valid 128-byte URL fit 196 pixels at scale at least 3; all four margins are at least four modules; replacing the payload changes the matrix. Export the actual C matrix as PGM and independently decode it using a pinned zxing-cpp test dependency; decoded text must equal the input.
- [x] Write asset tests for the union of fixed copy, ASCII, and supplied profile characters; missing source glyphs fail generation; three frames are 64-by-64 RGB565 and total at most 24,576 bytes. A second identical generation has identical source/hash outputs. Private paths are Git-ignored and absent from distributable source lists.
- [x] Implement the QR adapter using LVGL 9.5.0 qrcodegen with medium correction and bounded versions 1 through 9; add explicit quiet-zone layout. Do not rely on the widget's quiet-zone option to guarantee the spec's four modules. Link the existing encoder only once, and never edit managed_components.
- [x] Use lv_font_conv 1.5.3 and a licensed Noto Sans SC source with its OFL notice; record source URL/revision/hash and converter lock. Compile only the public subset by default. A local `PASSPORT_PRIVATE_ASSETS_DIR` CMake override selects personal generated fonts/avatars without committing them. Bind the generated fonts explicitly on every text-bearing widget.
- [x] Run `python tools/test_passport.py qr` and `python -m unittest discover -s tests -p test_passport_assets.py` until PASS. Retain decoded public QR previews and asset manifests under ignored build output. Personal avatar conversion remains pending actual user files; the neutral default is explicitly temporary.

## Task 5: Integrate the badge-first UI and application lifecycle

**Files:** Create `main/passport_ui.[ch]`, `main/passport_app.[ch]`, `tests/test_passport_runtime.c`, focused platform stubs under `tests/passport_stubs/`, `tools/preview_passport.c`, `tools/preview/CMakeLists.txt`, and `tools/render_passport_preview.py`; replace `main/main.c` boot flow. Update `main/CMakeLists.txt`, `sdkconfig.defaults`, `.gitignore`, and `tools/validate.sh` without removing existing baseline tests.

**Interfaces:** `esp_err_t passport_app_start(void)` initializes the application. `bool passport_ui_create(void)`, `bool passport_ui_render(const passport_model_t *, const passport_record_t *, int battery_soc, bool time_valid, int64_t utc_epoch_s)`, and `void passport_ui_destroy(void)` require initialized LVGL and its lock. `bool passport_app_post_input(passport_key_t, passport_key_event_t, uint64_t now_ms)` and worker-posted committed snapshots have bounded ownership. Pass Profile scroll limits from measured layout back to the model after rendering. The headless preview links the actual UI/assets against LVGL 9.5.0 with a software display, dumps RGB565 pixels, and uses Pillow only to encode PNG output.

- [x] Write failing runtime tests for badge-first startup, unavailable battery/NVS, ignored boot-held input until release, event-queue overflow recovery, failed task/queue/UI allocation, and repeated enter/exit of every page. Track live timers, page buffers, and owned requests; after each transition only the current page's resources may remain.
- [x] Implement new Badge/Profile/Homepage/Menu/Brightness/About screens with the approved colors, 22-pixel safe inset, page markers, unset states, and explicit Chinese font bindings. Render QR pixels into a small monochrome buffer with the layout from Task 4. Stop the avatar timer before removing its object; no baseline demo screen is reachable.
- [x] Initialize display/LVGL, input ownership, local storage, and optional battery in one startup owner. Use PRESS/RELEASE timestamps and the model tick for the 2-second hold instead of the BSP's 500 ms LONG event. Keep callbacks nonblocking; queue overflow sets a recovery signal rather than silently losing key state. Do not initialize Wi-Fi, BLE, or audio streaming.
- [x] Serialize brightness/profile requests through the storage worker; publish committed snapshots back to the application. A failed save restores the committed state and reports an accurate status. Set system time on a validated request; keep a per-boot validity flag and format UTC+08:00 without changing global host settings.
- [x] Register the focused tests in the existing static gate and compile the actual application adapters against fault-injection stubs. Run `python tools/test_passport.py all`, Python tool tests, and `bash ./tools/validate.sh --static` until PASS. Inspect startup/navigation source paths and a rendered public preview before calling UI implementation complete; real display acceptance stays separate.

## Task 6: Build, review, and prepare device acceptance

**Files:** Produce ignored firmware/debug bundles and logs; create paired `docs/assets/passport-os-v0.1-user-guide.md` documents and index entries. Update plan checkboxes with evidence references. Do not modify changelogs.

**Interfaces:** Consume all earlier interfaces and the activated native environment. Deliver the exact successful `build/FoloToy-AI-Passport-full.bin` bundle, symbols, segmented addresses, and checksums; never reuse an old image after a failed build.

- [x] Run the full `bash ./tools/validate.sh` gate and `git diff --check`; check source/lock diffs and verify the archived firmware using `python tools/archive_firmware.py verify <exact-successful-bundle>`. Save exit codes, tool versions, source revision/dirty state, flash offsets, and SHA-256 values. Review the completed branch using the review workflow before device writing. (2026-10-21: static gate via Git Bash PASS; firmware gate replicated step-for-step per `tools/validate.ps1` under Windows PowerShell 5.1 with an stderr-preference workaround; clean tree; archive `617aae031515fe6c26178a96dab6b4b48995804b4392abb6edd719414a085728` verified PASS, merged image 819,648 bytes; evidence in `build/task6-validate-full-20261021.log`; branch diff 83 files reviewed.)
- [ ] Inspect actual heap/largest-block logging and allocation bounds in the built application. Report Build, Host tests, Device tests, and Unverified separately. Do not mark hardware checks PASS from host tests or a successful compiler exit.
- [ ] Enumerate the target again; COM7 was observed earlier and is not hardcoded. Identify chip/flash and existing partition/data implications before any write. Present the exact validated artifact and data impact, then obtain authorization for this flash. Do not require full-chip readback, erase, upload, or publication.
- [ ] After authorized flashing, verify boot to Badge; local JSON import; reboot persistence; every short/long button path; unset time and USB time; Chinese names; QR scanning with a phone; brightness persistence; animation responsiveness and repeated navigation. Request personal details/assets only when needed for personalized acceptance. If hardware is unavailable or flashing is declined, keep Device tests NOT RUN and list the pending checks.

## Execution handoff

The user approved this plan and selected native execution in the existing session on 2026-10-20. Execution is in progress; the plan-scoped ledger records evidence and rulings. Design/plan approval is already recorded and does not need to be repeated.

## Execution evidence

Tasks 1–5 passed the native Windows gate: eight application C groups, eighteen Python tool/asset tests, actual LVGL glyph checks, 600 page transitions with allocation-failure injection, and independent QR decoding. Source checkpoint: `faf29bb`. ESP-IDF v5.5.3 is installed under `C:/Espressif`; the original E-drive location was superseded after measured write latency. `tools/validate.ps1` runs the same gates on native Windows. On 2026-10-21 the first Task 6 item completed: the full gate passed on source `4560a18` (clean tree, private assets injected), the isolated build archive `617aae031515fe6c26178a96dab6b4b48995804b4392abb6edd719414a085728` verified explicitly, merged image 819,648 bytes (bootloader@0x0, partition-table@0x8000, app@0x10000); log at `build/task6-validate-full-20261021.log`. Remaining: measured heap/largest-block review, flash authorization, and device acceptance.
