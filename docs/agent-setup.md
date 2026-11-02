<p align="right"><a href="agent-setup.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Agent Setup

## Copy-paste prompt

```text
Read docs/agent-setup.md in https://github.com/Zhenyu98/passport-os and follow it
to set up the toolchain and build Passport OS. Show me the plan before installing
anything, never flash a device without my approval, and report the build checks.
```

## Goal

Build a verified Passport OS firmware image (default or Muse) on the user's
machine, without touching their device or stored data unless they approve it.

## Prerequisites

- Git, Python 3.11+, and PowerShell 7 on Windows (Linux uses `tools/validate.sh`).
- For the default firmware: ESP-IDF 5.5.3 ([environment setup](development/engineering/environment-setup.md)).
- For the Muse firmware: ESP-IDF 6.0.1 and the Muse Gadget SDK at the pinned
  commit ([Muse guide](assets/passport-muse-integration.md#prerequisites)).
- A Muse SDK token only when the user wants to pair; the user enters it over USB.
  Never ask for it in chat or write it to a file in the repository.

## Steps

1. Read `AGENTS.md`, then make the required `passport-*` skills from `skills/`
   available as it describes. Start with `git status --short --branch`.
2. Install host requirements: `python -m pip install -r tools/requirements-passport-host.txt`.
3. Run the static gate: `python tools/prepare_passport_host.py`, then
   `bash tools/validate.sh --static` (or `tools/validate.ps1 -Mode Static` on Windows).
4. Build:
   - default firmware: `bash tools/validate.sh --firmware`;
   - Muse firmware: `pwsh -File tools/build_passport_muse.ps1` (add `-PublicAssets`
     for a shareable image).
5. Optional: give the badge the user's own animated character with
   `skills/passport-pet-avatar`.
6. Flash only through `skills/passport-device-test`, after the user names the
   port and approves. Prefer a segmented flash to keep stored data.

## Success signal

- Static gate prints `Repository checks: PASS` and the host test groups pass.
- The firmware build prints `Firmware layout: PASS` and `Merged firmware: PASS`.

## Safety rules

- Inspect first; ask before installing toolchains, flashing, erasing or publishing.
- Never print or commit tokens, Wi-Fi passwords, profile data or device logs.
- Personal assets stay in `assets/private/`, `private/` and `build/`, which are
  Git-ignored.
