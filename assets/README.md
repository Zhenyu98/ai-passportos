<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

The badge name and organization use `fonts/passport_font_24_bold.c`, generated
from the same pinned Noto Sans SC source at weight 700, 24 px and 2 bpp. The
shared 6,881-glyph inventory preserves Chinese coverage while keeping Flash
space for Muse. `passport/bold-font-manifest.json` records the source hash and
font parameters. Rebuild it with `tools/build_passport_bold_font.py`; custom
private inventories can be passed with `--glyph-manifest` and an ignored output.

## Images

Passport OS uses generated 16/20 px, 4 bpp Noto Sans SC subsets in `fonts/`.
The OFL license is retained beside them. The source is pinned to Google Fonts
revision `2894aab31764f10f29c421bdfd2340d3b382d384`; SHA-256
`a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da`.
`fonts/passport-font-converter.lock.yaml` pins lv_font_conv 1.5.3.
Run `python tools/build_passport_assets.py --font FONT --font-conv CONVERTER_JS
--node NODE --output assets` to reproduce the public assets. Fixed strings live
in `passport/ui-text.json`; `passport/manifest.json` records glyphs and hashes.
For personal glyphs/avatar, add `--profile build/profile.local.json` and optional
`--avatar IMAGE`, using `--output build/passport-assets`. Personal outputs must
remain ignored. The generated neutral 64 × 64 RGB565 avatar has three frames,
is explicitly temporary, and is original procedural artwork under the repository
license. QR modules use the pinned LVGL encoder and a four-module quiet zone.

Store reusable source images and generated display assets in `images/`.

Passport motion uses a public empty-set source `images/passport_motion.c` and
user-approved private 18-frame RGBA art in ignored `private/passport-motion/`.
The latter was generated from the supplied character with built-in image_gen on
2026-10-22 and has no assumed redistribution permission. Pack it with
`tools/build_passport_motion.py` into ignored `build/passport-assets`; see
[motion integration](../docs/assets/passport-motion.md) for formats and selection.
Approved night-view and cat-petting source frames live in ignored
`private/passport-scenes/`. Personal regeneration includes both packs; normal
personal builds use ignored `private/passport-build.json` at the repository root
to require all three dynamic choices. The personal artwork is not redistributed.

Muse Agent uses its own Nora character, separate from the identity card. The
user's screenshot is retained at ignored `private/muse-nora/reference.jpg`;
built-in `imagegen` produced `private/muse-nora/nora-states-v1.png` on 2026-10-24.
Her glasses, twin ponytails, cream bows and blue blouse are retained. No
redistribution permission is assumed. `tools/build_passport_agent_avatar.py`
packs eight 128×132 RGB565A8 frames into ignored
`build/passport-assets/images/passport_agent_avatar.c`: 405,504 Flash pixel
bytes, without a frame decoding buffer. Public builds use the neutral
`images/passport_agent_avatar.c`. Local `required_agent_avatar: true` and a
source checksum prevent silently losing Nora in personal builds.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
