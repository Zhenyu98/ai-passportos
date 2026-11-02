<p align="right"><a href="passport-os-v0.1-user-guide.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# AI PassportOS v0.1

The device starts on the research badge. UP/DOWN moves the highlight between
the two bottom buttons; OK enters the highlighted one — AGENT opens the
assistant page and QR shows the full-screen picture page (an uploaded card/QR
image, no status bar). On the picture page OK returns to the badge. The badge
adapts to the avatar automatically: tall standing scenes use a left avatar
column with the info list on the right; wide or near-square scenes use a large
centered avatar with compact info below. Real avatars breathe with a soft
opacity pulse; the built-in temporary avatar blinks. On the brightness page
UP/DOWN cycles through 30%, 60%, 90% and the value persists. Hold OK for two
seconds on the badge to open the hidden menu; on any other page the same hold
returns to the badge. Missing fields remain unset. The clock shows `--:--`
until synchronized on each boot, then UTC+08:00; battery failures show `--%`.
The badge has no separately powered clock chip, so turning it off with the power
button loses the time. The Muse firmware sets it again from network time servers
once it joins Wi-Fi; the default firmware needs the USB `time` command or the
phone-clock button on the configuration page.

## Assistant and hidden menu

The assistant page depends on the installed firmware. The default ESP-IDF
5.5.3 build has no Muse transport and reports that the Muse firmware is
required. The Muse firmware adds Bluetooth LE pairing, streamed voice notes and
text replies; see the [Muse integration guide](passport-muse-integration.md) for
the build, token and pairing steps.

- OK follows the action named on screen: it confirms a pending physical pairing,
  starts a recording, sends the recorded voice note, or starts a new attempt
  after an error.
- UP/DOWN scroll the reply or transcript. The task-results page shows the last
  completed reply; there OK saves the shown text as a local note when it fits
  the note limit. The notes page switches between the newest eight notes with
  UP/DOWN.
- Leaving the assistant cancels a recording that has not been sent. A turn
  already submitted continues, and its result remains available on the
  task-results page.
- Recordings are limited to 30 seconds.
- The hidden menu opens with a two-second OK hold on the badge, regardless of
  which bottom button is highlighted. On every other page the same hold returns
  to the badge. The menu has seven rows: display brightness, about, Wi-Fi
  config, game, task results, local notes and return. UP/DOWN move the row
  highlight and OK opens the highlighted row; the viewport scrolls through the
  seven rows four at a time.

## Wi-Fi configuration (phone)

On Muse firmware this entry first opens Network and Muse. It provides a physical
re-pairing confirmation even offline. Its profile editor uses the current LAN
and a temporary code displayed on the badge; see the [Muse guide](passport-muse-integration.md).
The hotspot instructions below apply to the default 5.5.3 firmware.

Select `Wi-Fi Config` in the hidden menu (hold OK for two seconds on the badge).
The badge opens the hotspot `Badge-XXXX` (open, five-minute timeout), answers
DNS for every name with its own address and redirects OS connectivity probes, so
joining the hotspot pops the settings page automatically on Android, iOS and
Windows (otherwise browse to `http://192.168.4.1/`).

The page edits the five profile fields, sets the clock from the phone, and
switches the avatar. The embedded pixel-scene gallery (the user's avatar pack
in private builds; the dynamic character and the Bund night view in public
builds) is listed as tappable
options; selecting one copies the scene into the avatar slot and the badge
shows it immediately. The picture page image (for example a WeChat or contact
QR card) is uploaded the same way: the page shows a 240x320 crop frame, which
the user drags and zooms over the chosen photo, and the browser converts that
exact crop to RGB565 before transfer. The device memory-maps it from flash and
shows it full-screen with zero RAM cost. `Clear picture` restores the empty
state and choosing the dynamic character restores the default avatar.
Saves run through the same single-writer storage path as USB with
compiled-glyph validation; unsupported characters are rejected. The hotspot
closes five minutes after it starts.

## USB configuration

Install `tools/requirements-passport-host.txt` with pip. Use an explicitly
verified port (`COM7` below is an example). The tool does not probe ports and
sets DTR/RTS false before opening. A UTF-8 local JSON file contains exactly five string
fields: `name`, `title`, `research`, `bio`, `homepage`; empty strings are allowed.
Byte limits, excluding terminators, are 64/96/192/512/128. Only research and bio
allow newlines; homepage is empty or a valid HTTP(S) URL.

```text
python tools/configure_passport.py --port COM7 info
python tools/configure_passport.py --port COM7 set --profile build/profile.local.json
python tools/configure_passport.py --port COM7 get
python tools/configure_passport.py --port COM7 time --now
```

A PC-side helper `tools/passport_web.py` runs the same pipeline locally
(profile form, scene picker over the avatar pack, custom uploads) and renders
previews; it is a development aid and never runs on the device.

Import checks compiled glyph coverage. New Chinese characters require a rebuilt
font subset; unsupported text is rejected. Keep personal input and generated
assets/previews in ignored `build/`. Profile values are imported separately into
the application's own NVS record; failed saves retain committed data. No NVS
erase is attempted to recover errors.

## Reproduction and acceptance

Follow the [asset instructions](../../assets/README.md) for pinned fonts/license
and local assets. CMake defaults to public assets; `PASSPORT_PRIVATE_ASSETS_DIR`
selects ignored local output. Prepare hash-pinned host sources with `python
tools/prepare_passport_host.py`, then run `bash tools/validate.sh --static`.
The gate includes actual LVGL rendering, compiled glyph checks, 600 transitions,
stable pool usage and independently decoded QR pixels.

The user firmware appends a 512 KB `avatar` data partition after the factory
app; NVS offsets are unchanged, so segmented flashing preserves stored data.

An activated native Windows ESP-IDF 5.5.3 shell can use `tools/validate.ps1 -Mode
All -HostPython HOST_PYTHON -BuildDirectory NEW_DIRECTORY`, optionally adding
`-PrivateAssetsDirectory LOCAL_ASSETS` and `-BashExecutable GIT_BASH`. It runs the
same static gate, build, merge, image verifier and archive creator, avoiding the
SDK's unsupported MSYS path. Linux uses `./tools/validate.sh`.

The archive retains matching ELF/MAP, component images, flash arguments and
hashes. Startup logs report free heap and largest block. Actual board values,
stack margins, Chinese display, controls, persistence and phone scanning require
device acceptance; host success is not hardware acceptance.

Approve the exact verified build before flashing. Inspect the device's existing
partition table first. Compatible segmented flashing can preserve NVS; a merged
image from offset zero spans NVS and can reset data. No full-chip erase, original
firmware backup, upload or publication is required. The passive NTAG213 tag on
the back can be written once from a phone to mirror the homepage URL for
tap-to-share; it is independent of the firmware.


## Set up NFC tap-to-share on iPhone

Saving the homepage field does not write the passive NTAG213 on the back. The
MCU has no interface for writing this tag. A direct iPhone scan without a usable
URL record can therefore fail even when the firmware and homepage are working.
An empty or incompatible record is a hypothesis until a phone reader confirms it.

Use a phone NFC reader to inspect the tag first. To initialize tap-to-share, use
NFC Tools: **Write → Add a record → URL/URI**, enter the same complete `https://`
homepage URL, and write it with the phone's NFC antenna close to the rear NFC
mark. Read it back to confirm the URL, close the reader, then try background
scanning with the screen on. On supported iPhones (XS and later), background
reading requires a supported NDEF URI record; plain text containing a URL is
not equivalent. See [Apple's background-reading requirements](https://developer.apple.com/documentation/corenfc/adding-support-for-background-tag-reading)
and [the NFC Tools writing guide](https://www.wakdev.com/en/knowledge-base/how-to-guides/how-to-write-a-link-url-on-an-nfc-chip.html).

If the app cannot detect the tag at all, check antenna alignment and retry
without a case or intervening cards; record the iPhone model and app result.
That outcome needs an RF/tag check, and changing the badge firmware will not
write or repair this independent passive tag. Changing the homepage later
requires writing the NFC URL again if both should remain the same.
