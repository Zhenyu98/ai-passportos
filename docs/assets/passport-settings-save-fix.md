[简体中文](passport-settings-save-fix.zh_CN.md) · **English**

# Settings save repair

The phone's Badge hotspot page previously displayed the internal `unsupported`
code when any profile character was absent from the tiny compiled font subset.
This rejection happened before NVS writing. Generating a subset from the initial
profile made ordinary later name changes unexpectedly fail.

Default fonts now include the 6,763 GB2312 Han characters, common punctuation,
ASCII and UI copy (6,881 glyphs in the public build). Personal builds still add
their own characters. Both font sizes use LVGL's large glyph-index format because
the 20 px bitmap exceeds the small format's 1 MB index range. Glyphs remain in
Flash; the rendering pool stays at 32 KB. Uncovered rare characters are rejected
with a field name, character and Unicode codepoint, rather than a bare error code.

The configuration path is divided into reusable modules:

- `passport_json`: bounded UTF-8 JSON parsing, duplicate-key checks and numeric bounds.
- `passport_settings`: partial profile updates and human-readable error messages.
- `passport_profile_check`: shared validation and exact missing-glyph diagnostics.
- `passport_app`: one owned request slot and storage worker for both USB and Wi-Fi.

The hotspot owns its callback structure instead of retaining a caller's stack
address. Time synchronization follows a successful profile commit; a clock error
does not claim that the saved profile failed. HTTP reads have bounded retries.
Pending timed-out requests keep their slot until acknowledged, preventing the
next transport from consuming an earlier operation's result.

USB `info` adds `glyphs_complete`. Inventories of up to 400 glyphs remain inline;
larger fonts return an empty list and `false`, keeping replies inside the 4 KB
frame (reduced from 8 KB to free internal RAM for the Muse firmware).
The accompanying host utility then delegates coverage validation to the device.
Use the matching utility for firmware with the larger font.

Regression checks cover common Chinese names, malformed/partial settings, exact
missing characters, large-font USB replies and stale cross-transport requests.
The actual LVGL fixture checks every advertised glyph at both sizes and renders
a QR fixture through the current full-screen picture page. It rejects missing
fresh output instead of reading an obsolete screenshot. Avatar accounting now
reports all three frames, correcting the prior single-frame metadata total.

Firmware and real phone acceptance remain separate. Flash only the explicitly
approved verified artifact; preserve NVS/media through compatible segmented
flashing and never erase the device as a response to `unsupported`.

## Verified candidate (2026-10-22)

Source `2362b95`, application version `0.1.1`. The complete native gate passed:
9 application C groups, 20 Python tests, all 6,881 glyphs at both sizes,
500 actual LVGL page transitions, independent QR decoding, image layout and
archive verification. Independent review found no new actionable issue.

Merged-image SHA-256: `332ed71b56552bcddd584f61ba8bff9eb8066b9da6cdd6d0f807040f83645b70`.
Matching ELF SHA-256: `810cb27b2816a7938994524865bea519c1f1729c5621a8eb962f87c00e2d5c09`.
Application image SHA-256: `3e57ae1b23ec1e32cbb7c0cd450089adba4980b2f453766d1f57401166b701b6`.
The application is 5,932,800 bytes in a 7,798,784-byte partition. The read-only
device partition table exactly matches the archive. An authorized update can
write only the application at `0x10000`, preserving NVS and media.

Device acceptance is partial; measured results follow below. Phone Save and
Chinese screen rendering still need confirmation, as do hotspot heap/stack margins.

## Authorized device update (2026-10-22)

The approved archived application was written to COM7 at `0x10000`; esptool
verified the data hash. Partition table, NVS and media were outside the write
range. A controlled reboot returned USB version `0.1.1`; its boot ELF prefix
matched `810cb27b2`. The eight-second startup capture contained no observed crash
marker. Profile and brightness matched the pre-update snapshot exactly. Startup
free heap was 121,456 bytes and the largest block was 106,496 bytes.

These are measured boot/USB/data-retention results. Phone Save and visual
confirmation are still being requested. Private snapshots and raw logs remain
under ignored `build/device-tests/`.

## Media readback repair (0.1.2)

The stored-media reader computed its CRC with the wrong initial value, so every
avatar scene and full-screen picture written to the media partition failed
validation and silently fell back to the compiled default: the badge never
changed after selecting a scene, and the picture page kept showing its
"no image yet" hint. The reader now uses the same zlib-compatible initial value
as the writer. A host media test checks the exact CRC vectors, deliberately
unaligned chunk streaming, readback, and rejection of corrupted payloads.

Two companion fixes ship with it. The transient status heading (the save
confirmation) expires after three seconds instead of staying on screen. Cached
pixels are released lazily on the next load, so a rendering frame never reads
freed or unmapped memory; storage also fails closed when its mutex cannot be
created. An avatar loaded while the hotspot holds heap falls back to a flash
mapping instead of disappearing.

## Verified 0.1.2 build and device update (2026-10-22)

The complete gate passed on the working tree above: nine application C groups
including the new media test, the Python suite, all 6,881 glyphs at both sizes,
500 actual LVGL page transitions, independent QR decoding and archive
verification. Merged-image SHA-256 is
`075033cc334d89afcd4df48079656d2dd02c75de99ca7e2823a39e64f0b2cc08`, app-image
SHA-256 is `10354573fd3c5ea1f462936b12f679d7be753f3b7b03c7731acb4dee6109ab1f`,
and matching ELF SHA-256 is `000499f4841e8b202ffe0f848a8b58aad7b1119fb01251f41a00d7c35d2f38a2`.
The application is 5,933,504 bytes in a 7,798,784-byte partition (24% free).

The archived application was written to COM7 at `0x10000`; esptool verified the
hash. The partition table and the 512 KB media partition stayed outside the
write range, so the stored avatar scene and picture were preserved and now pass
validation again. The device booted the new build (compile time 10:32:55, ELF
prefix `000499f48`); startup free heap was 86,768 bytes with a 69,632-byte
largest block.
