---
name: passport-pet-avatar
description: Put the user's own animated character on the badge. Hatch a pet with Codex's hatch-pet skill, convert its atlas into the private 76x150 idle/focus/nod motion pack, and build firmware that uses it. Not for static photo avatars, the hotspot avatar gallery, or publishing personal art.
---

<p align="right"><a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Animated Badge Avatar from a Codex Pet

The public firmware ships with a default motion character. This workflow
replaces it, on the user's machine only, with a character the user hatches in
Codex. Resolve the checkout with `git rev-parse --show-toplevel`; paths below are
relative to it. Personal art never leaves Git-ignored locations.

## 1. Hatch the pet in Codex

Use Codex's built-in `$hatch-pet` skill (the Pets feature). It produces a v2
pet in `~/.codex/pets/<id>/`: `pet.json` plus an 8-column atlas of transparent
192x208 cells. Ask for a character that stays readable when it shrinks to a
76x150 slot:

```text
Use $hatch-pet to hatch a pixel-art pet from this photo of me: full body,
standing, feet on the ground, simple silhouette, roughly 2:3 portrait
proportions, no large props beside the body.
```

Wide or chibi characters work but render smaller, since the slot is portrait.
Approve the idle, review and waving rows carefully; the badge uses only those.

## 2. Convert and install

Install the host requirements once (`python -m pip install -r tools/requirements-passport-host.txt`), then:

```text
python tools/import_codex_pet.py --pet ~/.codex/pets/<id> --install
```

- `idle` → idle, `review` → focus, `waving` → nod. Rows shorter than six frames
  play as a ping-pong loop. Change the mapping with `--focus-row` / `--nod-row`
  (any hatch-pet row name).
- All 18 frames share one crop, scale and feet baseline, so the character never
  pops in size between states. The pack goes to the Git-ignored
  `assets/private/passport-motion/`.
- `--install` compiles the pack into `build/passport-assets/` (seeded from the
  public assets when empty) and writes `private/passport-build.json` if it does
  not exist yet. An existing personal configuration is left untouched; then run
  `python tools/build_passport_motion.py --pack assets/private/passport-motion --output <asset_dir>`
  yourself, using the `asset_dir` it names.

The command fails without writing the build tree if the atlas geometry is wrong
or a frame would clip; fix the pet in Codex rather than editing frames by hand.

## 3. Build and test

Build as usual without public-asset overrides: `./tools/validate.sh --firmware`
for the default firmware, or `pwsh -File tools/build_passport_muse.ps1` (no
`-PublicAssets`) for the Muse firmware. CMake stops with "Required motion
assets missing" when the configuration points at an incomplete asset tree.
Flash only with `passport-device-test` and the user's authorization, then check
the badge: idle loop on the identity card, focus and nod on button presses.

To return to the default character, delete `private/passport-build.json` (or
build with `-PublicAssets`).

## Boundaries

- Never commit `assets/private/`, `private/`, `build/`, the source photo or the
  pet atlas; run `python tools/check_repo.py` before any commit.
- Do not publish firmware built from personal assets; community releases use the
  public assets.
- A successful build is not device acceptance; size and motion need a look on
  the real screen.
