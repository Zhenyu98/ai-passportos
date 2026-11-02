[简体中文](passport-os-v0.1-design.zh_CN.md) · **English**

# AI PassportOS v0.1 design

Date: 2026-10-20. Status: approved by the user on 2026-10-20; application implementation has not started. See the [implementation plan](passport-os-v0.1-plan.md).

## Purpose and scope

Build a wearable multi-function identity badge that starts on the badge and remains useful offline. Deliver the badge, Profile, personal-homepage QR code, persistent local data, and a small animated avatar. The user has deferred XiaoZhi voice and MCP integration in favor of a later Muse integration.

Voice notes remain a product requirement, but their former XiaoZhi transcription dependency must be replaced before implementation and acceptance. Muse, transcription, phone BLE editing, and laboratory control are outside this increment. Do not describe this increment as the complete original voice-enabled v0.1.

Names, titles, research interests, photos, and homepage addresses must come from the user. Empty fields have explicit unset states. A missing homepage never produces a fabricated personal QR code.

## Foundation and alternatives

Use `FoloToy/ai-passport` at commit `3d685a90bdb96dd8526d84edde1e63fec25b460e`: ESP-IDF 5.5.3, ESP32-C3, 8 MB Flash, no PSRAM, and the existing LVGL 9.5 dependency lock. Preserve the BSP, pin definitions, and USB Serial/JTAG console. Work on `feature/passport-os-v0.1`.

The official hardware baseline is preferred because it supplies the display, buttons, and battery APIs while allowing an independent application. The XiaoZhi distribution retains postponed voice dependencies and uses a different SDK baseline. Rewriting drivers adds hardware risk without improving this increment. Implement new application screens under `main`; do not reuse the baseline test menu or its visual shell.

## Pages and controls

Use a 240-by-320 portrait layout: dark navy, light text, a restrained teal accent, and a white QR panel. Keep important content at least 22 pixels from the left/right edges and outside rounded corners. Adjust spacing during rendering checks if needed for readability.

| Page | Content |
| --- | --- |
| Badge | Header time and battery; centered 64-by-64 avatar; name, role, wrapped organization/tag line; page indicator. |
| Profile | Name, title, research interests, biography; button scrolling for long content. |
| Homepage | High-contrast QR code and readable destination host, or an explicit unset state. |
| Menu | Brightness, About, Return; no inactive voice-service entry. |
| Brightness / About | Persisted 30%, 60%, 90% brightness, default 60%; application version and build information without identity secrets. |

| Input | Badge / Profile / Homepage | Menu and child pages |
| --- | --- | --- |
| UP / DOWN click | Previous / next page; scroll a long Profile to its boundary before changing page. | Move selection; adjust brightness on its page. |
| OK click | Open menu, remembering the originating page. | Select item; About returns to menu; Return restores the originating page. |
| OK hold, 2 seconds | Return to Badge. | Return to Badge. |
| UP / DOWN hold | No additional action in this increment. | No additional action in this increment. |

A long press produces one action and suppresses the release click. A double click has the same total navigation effect as two clicks, without an extra action. Button callbacks only enqueue bounded events; they never access LVGL or NVS.

Show `--:--` until time is explicitly set during the current boot; display set time in Asia/Shanghai (UTC+08:00). A host utility may set it over USB; offline display does not imply accurate time after complete power loss. Unavailable battery data displays `--` and does not block the badge.

## Data, fonts, avatar, and QR

Store a schema-versioned complete profile and display settings in a dedicated `passport` NVS namespace. Validate replacements before committing; rejected imports and storage errors preserve the last valid record. Do not erase other namespaces or the entire NVS partition to recover from an application-data error.

Use an ignored local UTF-8 JSON file and a USB configuration utility for initial profile editing. Field limits, excluding terminating NUL: `name` 64 bytes, `title` 96, `research` 192, `bio` 512, `homepage` 128. Reject invalid UTF-8, over-limit fields, unsupported control characters, and malformed nonempty URLs. A homepage must use `https` or `http` and contain a host. Do not upload personal data or log credentials.

Reuse the USB console with bounded line-delimited JSON requests, request identifiers, and a distinct response prefix. Support profile read, validated replacement, and setting the current time. Require explicit target-port selection and never probe unrelated Bluetooth ports. Ordinary logs must not be mistaken for protocol responses.

Generate a reproducible font subset from fixed UI copy and the configured profile. Before importing changed text, the utility checks the firmware's advertised glyph inventory. Unsupported characters produce a clear error without overwriting the profile; adding characters requires rebuilding the font subset. Keep private JSON and personal generated inventories ignored. Explicit unset states allow a development build before personal details are available.

Preprocess the avatar into three 64-by-64 RGB565 frames in Flash, at most 24,576 bytes of frame pixels. Use a clearly temporary neutral avatar until the user supplies one. Sparse blink frames use a timer; do not add a GIF decoder or a full-screen frame buffer.

Generate real QR modules from the validated URL: black on white, four-module quiet zone, integer scaling, and at least three pixels per module inside a 196-by-196 available square including the quiet zone. Generation/fitting failure shows an explicit error, never a decorative matrix or a substituted URL. Validate a public test URL on the host and the final personal URL on the real screen.

## Architecture and failure handling

Separate the pure input/profile model, NVS adapter, USB handler, LVGL presentation, and compiled assets. A single application owner serializes state changes; workers handle bounded storage/USB work. LVGL access occurs in its task or under `bsp_lvgl_lock()`. Stop page timers and producers before deleting UI objects.

Retain the existing small display buffer and begin with the baseline LVGL pool. Measure total heap, largest free block, and rendering before changing allocations. Keep radio/audio stacks inactive. Store avatar frames in Flash and activate only the selected frame descriptor.

Missing/corrupt application data produces explicit unset fields and an accurate diagnostic. Storage failures must not report success. Malformed USB requests must not crash or change state. Animation must preserve navigation and USB responsiveness.

## Verification and delivery

1. Prepare the five required project skills and exact ESP-IDF 5.5.3 environment; retain an unmodified baseline build result before application changes. Use native Windows, without unrequested WSL migration or global configuration edits.
2. Test input/page transitions, UTF-8/URL validation, replacements, malformed/oversize USB requests, unsupported glyphs, storage failures, and QR generation with focused host checks.
3. Run repository static and firmware gates. Retain source commit, logs, binaries, matching symbols, and SHA-256. A merged image does not authorize flashing.
4. Before device writes, identify the actual target, existing/proposed partitions, exact artifact, and stored-data impact; obtain authorization for that flash. Preserve configuration where the selected layout permits and explain incompatible changes. No unrequested full-chip erase. Original firmware readback is optional, not mandatory.
5. On the device, verify cold boot to Badge, offline display, persistence after reboot, controls, Chinese glyphs, real phone scanning, brightness persistence, animation responsiveness, and missing-data behavior.

Report Build, Host tests, Device tests, and Unverified separately. Placeholder builds are development builds; personal QR/avatar acceptance remains pending until actual data and device checks are available.

## Observed starting state

Official source is downloaded. Previous custom source/patch files are absent from the handoff. This Windows x64 shell has Python 3.14.6 and Git; ESP-IDF, CMake, Ninja, and a host C compiler were not found on PATH or in the few standard ESP-IDF locations inspected. Other installations may exist.

The user confirms USB cable connection, power on, and a normally displayed interface. Initial enumeration showed only Bluetooth COM3/4/5/6. A later check identifies an Espressif USB Serial/JTAG interface, VID 303A / PID 1001, on COM7 with its driver started. The physical target's chip, current firmware, and Flash layout have not yet been queried. No baseline build, application host tests, flash, or device acceptance has run for this design.
