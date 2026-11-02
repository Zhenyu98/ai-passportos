<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

<h1 align="center">AI PassportOS</h1>

<p align="center">
  <strong>Open firmware that turns the FoloToy AI Passport into a multi-function AI badge.</strong>
</p>

<p align="center">
  <strong>Animated identity card</strong> ·
  <strong>Muse AI assistant</strong> ·
  <strong>Landscape mini game</strong> ·
  <strong>Your own character</strong>
</p>

<p align="center">
  <a href="https://github.com/Zhenyu98/ai-passportos/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/Zhenyu98/ai-passportos?style=flat-square&logo=github"></a>
  <a href="LICENSE"><img alt="License MIT" src="https://img.shields.io/badge/License-MIT-64748b?style=flat-square"></a>
  <img alt="ESP32-C3" src="https://img.shields.io/badge/ESP32--C3-no_PSRAM-2563eb?style=flat-square">
  <img alt="ESP-IDF" src="https://img.shields.io/badge/ESP--IDF-5.5.3_%7C_6.0.1-14b8a6?style=flat-square">
</p>

<p align="center">
  <img src="assets/images/ai-passportos-hero.png" alt="AI PassportOS screens: animated identity card, Muse reply, Bund night-view avatar, landscape mini game" width="94%">
</p>

<p align="center"><sub>All screens are host renders of the real firmware UI. The sample profile is fictional.</sub></p>

## What it does

One badge, several jobs: it boots into an animated identity card, one button
press opens your Muse AI assistant, and in a quiet moment you can turn it
sideways for a quick game. Everything is driven by the three buttons on the device.

### Animated identity card

Name, role, organization and bio in Chinese or English, with a live clock and
battery level. The avatar is a pixel character that breathes, blinks and nods,
or an animated banner such as the Bund night view; the layout adapts to it. The
QR button opens a full-screen QR code or business-card image.

### Muse AI assistant

The Muse integration is built on Meta's official [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk),
pinned to commit [`e6fe517`](https://github.com/facebookincubator/muse-gadget-sdk/tree/e6fe5174092702ffddae6f90759a38d55a4ebae1) and fetched at build time rather than
vendored here.
After Bluetooth LE pairing in the Muse app, the badge becomes a pocket entry to
Muse: press OK to talk, read a reply preview on screen, and keep the full answer
on your phone. Replies can be saved as local notes; the newest eight stay on the badge.

<p align="center">
  <img src="assets/images/showcase-muse.png" alt="Muse assistant: listening, reply, task result, local notes" width="94%">
</p>

Muse can also operate the badge through a small set of device tools: read
status, set brightness, save a note and switch pages. Nothing else.

### Landscape mini game

The hidden menu holds a side-scrolling platform game. Turn the badge sideways:
UP/DOWN move, OK jumps.

<p align="center">
  <img src="assets/images/showcase-game.png" alt="Mini game: in play and title screen" width="66%">
</p>

### Also included

| Feature | Details |
| --- | --- |
| Configuration | Phone hotspot portal (default firmware) or LAN page with a one-time code (Muse firmware), plus a USB JSON protocol |
| Network time | The Muse firmware sets the clock from NTP as soon as Wi-Fi is up |
| Custom character | Hatch a character with Codex's `$hatch-pet` and convert it with one command ([skill](skills/passport-pet-avatar/SKILL.md)) |

## Two firmware builds

| | Default firmware | Muse firmware |
| --- | --- | --- |
| Toolchain | ESP-IDF 5.5.3, repository root | ESP-IDF 6.0.1, `muse/` |
| Agent page | Explains that the Muse firmware is required | Muse pairing, voice notes, replies |
| Profile editing | Open hotspot `Badge-XXXX` | LAN page with a one-time access code |

Both keep the profile, images and settings in the same NVS records and Flash
partition, so switching firmware with a segmented flash keeps your data.

## Quick start

1. Download `FoloToy-AI-Passport-full.bin` from the
   [releases](https://github.com/Zhenyu98/ai-passportos/releases), or build it.
2. Flash the merged image at `0x0` (this resets stored data):

   ```text
   python -m esptool --chip esp32c3 -p <PORT> write-flash 0x0 FoloToy-AI-Passport-full.bin
   ```

3. Hold OK for two seconds on the identity card and choose **Wi-Fi config**. On
   the default firmware the phone joins the `Badge-XXXX` hotspot; on the Muse
   firmware, pair in the Muse app first, then choose **Open profile
   configuration**. Set your profile, avatar and QR image from that page.

Building the Muse firmware, setting the Muse SDK token and pairing are covered
step by step in the [Muse integration guide](docs/assets/passport-muse-integration.md).
Button behaviour and the USB protocol are in the
[user guide](docs/assets/passport-os-v0.1-user-guide.md).

## Agent setup

The repository is set up for AI-assisted development (`AGENTS.md`, `skills/`).
Paste this into Codex, Claude Code or another coding agent:

```text
Read docs/agent-setup.md in https://github.com/Zhenyu98/ai-passportos and follow it
to set up the toolchain and build AI PassportOS. Show me the plan before installing
anything, never flash a device without my approval, and report the build checks.
```

See [docs/agent-setup.md](docs/agent-setup.md) for the full checklist.

## Known limits

- Muse replies are text. Reading them aloud needs a text-to-speech endpoint of
  your own ([voice guide](docs/assets/passport-agent-voice.md)); a Windows bridge
  is included for local testing.
- The ESP32-C3 has no PSRAM. The Muse control session fits after the memory
  savings in this release, but a full voice round trip has not yet been verified
  on hardware.
- Long replies, Muse history and account tasks stay in the Muse phone app; the
  badge keeps a bounded preview.

## Acknowledgements

- [FoloToy AI Passport](https://github.com/FoloToy/ai-passport) — hardware, BSP and
  the open firmware template this project builds on (MIT).
- [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk) by Meta —
  pairing, Wi-Fi and the encrypted control session (Apache-2.0). It is fetched at a
  pinned commit and patched at build time, not vendored. Use of Muse is subject to
  the [Gadget SDK terms](https://gadgets.muse.ai/sdk-terms).
- [ESP-IDF](https://github.com/espressif/esp-idf), [LVGL](https://lvgl.io) and
  [Noto Sans SC](https://fonts.google.com/noto/specimen/Noto+Sans+SC) (OFL).

## License

MIT, see [LICENSE](LICENSE). Third-party components keep their own licenses.
