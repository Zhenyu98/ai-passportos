[简体中文](passport-motion.zh_CN.md) · **English**

# Passport OS character motion

The approved calm character now has a pre-rendered animation player: occasional
breathing/blinking, attention to the selected button, and one nod after returning
to the badge or a successful operation while the animated avatar is visible.
Navigation never waits for the gesture, and a new focus replaces the old gesture.

## Select the character

This private build contains the approved eighteen-frame character. With no saved
static avatar it is the default. An existing uploaded scene keeps precedence, so
an update preserves the user's selection. Hold OK for two seconds on the badge
to open the hidden menu, choose Wi-Fi configuration, connect to the displayed
hotspot, and choose **Dynamic character (blink and respond)**,
**Bund night view (animated)** or **Petting cat (animated)** in the scene grid.
Selection stores a scene ID in the avatar-slot header sector and survives reboot;
profile/NVS and the full-screen picture slot are unchanged. Choosing a regular
scene returns to a static avatar.

The public source build retains its neutral three-frame placeholder. It does not
redistribute the user's character artwork. A public build without the private pack
does not offer the dynamic-character entry.

## Runtime and layout

- `main/passport_animation.c` is pure C, with no allocation or LVGL dependency.
  Time is a wrapping 32-bit monotonic millisecond value. Catch-up work is bounded.
- `main/passport_ui.c` runs a 50 ms timer only while the character is visible.
  It swaps image sources without moving/scaling/fading the sprite. Placeholder
  art can blink at 100 ms; uploaded static scenes do not run an avatar timer.
- Focus updates recolor existing button objects and retain the avatar object.
  Leaving a page cancels the player and deletes its timer before deleting UI.
- The avatar stays in the current 76×150 left column. AGENT is a near downward
  attention gesture; QR uses the stronger right/down gaze. It does not map the
  left button to a screen-left gaze. A nod is 440 ms including return to neutral.
- The full-body source poses for left/right gaze remain in the asset set;
  unused leftward poses do not imply pointer or sensor tracking.
- Night view uses a 216×112 wide stage; petting cat uses the 76×150 portrait
  stage. Each has six calm looping frames. Selection feedback stays immediate
  without restarting these loops. The default character retains its interactive
  attention/nod gestures. Game entry stops the avatar timer before deleting UI.

## Assets and rebuild

Local source frames and `animation-manifest.json` live in ignored
`assets/private/passport-motion/`. They derive from the user's supplied default
pixel character and the built-in image_gen animation strips approved on
2026-10-22. No redistribution of that identity artwork is assumed. Public assets
include only the empty motion-set source `assets/images/passport_motion.c`.

Generate the private source alongside an existing private font/gallery set:

```text
python tools/build_passport_motion.py --pack assets/private/passport-motion --scene-pack assets/private/passport-scenes --output build/passport-assets
```

The output asset directory must already contain the private fonts, strings,
avatar and gallery when used for a firmware build. The packer does not regenerate
them. It checks frame count, RGBA/76×150 format, transparency, safe edges and foot
baseline before replacing its generated source. Payloads use an RGB565 little-
endian plane followed by A8. Eighteen frame descriptors reuse fourteen unique
buffers: 478,800 pixel bytes (467.6 KiB) in Flash, without a full-frame decode
buffer. The two new scenes add 412,920 unique pixel bytes: five RGB565 night
images and five RGB565A8 cat images; the loop's last frame reuses its first.
All three scenes together occupy 891,720 pixel bytes, without runtime decoding.

### Personal-build regression guard (0.1.4)

The merged firmware had retained the player but compiled the public empty set
(`passport_motion_count=0`) because its build omitted the personal asset override.
Personal regeneration now incorporates local motion sources automatically. The
local, ignored `private/passport-build.json` contains:

```json
{"asset_dir":"build/passport-assets","required_motion_scenes":3}
```

Normal firmware builds read these settings, require all three scenes and verify
the generated source hash. Missing/incomplete data fails clearly instead of
silently producing a static personal build. Settings, manifest and generated
source are configuration dependencies, including for incremental builds. Use
`-DPASSPORT_USE_LOCAL_ASSETS=OFF` explicitly for a public build. Private source
scene frames live in ignored `assets/private/passport-scenes/`; their source is
the user's approved image_gen night/cat samples from 2026-10-22.

The main asset generator accepts `--motion-pack` and `--scene-pack`; it detects
these local packs for personal builds. It also records the motion source in the
asset manifest. A private LVGL preview can require the resources using
`--require-motion-scenes 3`, so tests cannot silently skip the feature.

This is compiled-in animation. The PAV1 single-image upload protocol remains
compatible for static images. It cannot accept this frame set or a GIF. Built-in
selection adds a 16-byte header variant, format 3: reserved bytes hold scene ID,
version 1 and zero; CRC covers the preceding twelve header bytes. Stable IDs are
1 default, 2 night and 3 cat. Old firmware ignores this variant and falls back;
new firmware still reads existing static images. No I8 decoder is enabled.

## Validation and device acceptance

The timing tests cover blink/breath, frame boundaries, latest-input preemption,
hide, long stalls and tick wrap. The packer tests cover repeatability, wrong/
empty/missing/clipped frames, baseline drift and private-output protection. The
actual LVGL tests exercise both public and private builds at a 32 KB LVGL pool,
100 focus changes, source changes, timer removal, static fallback, nod captures,
600 page transitions (including the merged runner game), font coverage and
independent exact QR decoding. Tests also cover scene-selector CRC, reboot/cache
recreation, active-upload exclusion, picture-slot preservation and build guards.

Current build identity, gate results and firmware/ELF hashes are recorded in the
ignored `build/scenes-handoff.json` for 0.1.4 after validation (`motion-handoff.json`
retains the earlier 0.1.3 result). A host render or successful
build does not prove physical timing, power use, SPI bandwidth, Wi-Fi concurrency
or on-device appearance. Check those on hardware, including restoring a static
scene and selecting the dynamic character again. Flashing requires approval for
the exact candidate and a compatible device partition layout.

### 2026-10-22 device checkpoint (0.1.3)

After user approval, the live COM7 partition table matched the verified candidate.
Only the 6,414,080-byte application was written at `0x10000`, with the write hash
verified; NVS, PHY and media regions were not written. A controlled reboot and
35-second observation matched the retained ELF, showed one boot with no crash/
watchdog signatures, and returned a successful read-only profile response.
The serial port was closed afterward. The user then confirmed idle blinking,
up/down selection attention and the nod when returning from the menu all work
normally on the device. Device acceptance for these three motion behaviors is PASS;
measured Wi-Fi peak memory, long-run stability and power/wake testing remain open.


### 2026-10-22 scene firmware startup checkpoint (0.1.4)

The user approved the exact archived candidate whose application SHA-256 starts
`5308afc88ec66465`. COM7's freshly read partition table matched the archive.
Only the 6,833,808-byte application was written at `0x10000`; esptool verified
the write hash. Sector erasure ended before `0x695000`, preserving NVS, PHY and
the media partition starting at `0x780000`. Application Flash headroom is
964,976 bytes (942.4 KiB, 12.4%).

A controlled reboot followed by 35 seconds of observation matched ELF marker
`3ee87a1b`, showed one boot and no crash/watchdog signatures, and successfully
returned the read-only USB profile query. Startup free heap was 120,976 bytes,
with a largest free block of 106,496 bytes; this is not a Wi-Fi peak-memory
measurement. The serial port was closed afterward. The user subsequently confirmed all three animations, selected-scene persistence
after reboot and runner entry/exit work normally. Device acceptance for these
behaviors is PASS; Wi-Fi peak memory, long-run stability and power/wake
performance remain unverified. The same acceptance reported separate text-scroll
and NFC-setup issues, tracked in the 0.1.5 follow-up.
