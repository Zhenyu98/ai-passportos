<p align="right"><a href="passport-os-v0.1-visual-spec.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Passport OS visual spec

Reference for anyone changing the badge UI. Keep new screens inside these
tokens; the geometry lives as `#define` constants at the top of
`main/passport_ui.c` (visual tokens), so changing a layout value is one edit.

## Palette

| Token | Hex | Use |
| --- | --- | --- |
| Background top/bottom | `0x0e2a4a` → `0x0a1c33` | Vertical screen gradient |
| Panel | `0x0c1d3a` | Cards, buttons, menu rows |
| Track | `0x1a4a6e` | Borders, inactive bars |
| Ink | `0xf4faff` | Primary text |
| Muted | `0x8fa3c0` | Secondary text, unselected buttons |
| Neon | `0x3dff9e` | Accent, selection, title color |
| On-canvas text | `0x0a1c33` / `0x4a5a76` | Text on the white picture canvas |
| Nora canvas / subtitle | `0xffffff` / `0x222222` | Full-screen assistant, with gray necessary notices |
| Battery low / mid | `0xff5a5a` / `0xffcf3f` | Below 20% / below 50%; else neon |

## Fonts

| Widget | Font |
| --- | --- |
| Clock | Montserrat 28 |
| Weekday, battery, buttons, icons, TEMP | Montserrat 14 |
| Badge name, organization/identity | `passport_font_24_bold`, Noto Sans SC weight 700, 2 bpp |
| Research, headings, menu rows, Nora subtitles | `passport_font_20` |
| Hints and notices | `passport_font_16` |

The profile fonts are generated subsets (GB2312 in personal builds); USB saves
reject characters outside the compiled set.

## Regions

- Status bar `y 0–30`: clock + weekday (left, adjacent), battery percent +
  battery icon (right). Hidden on picture, game and Nora pages.
- Body `y 30–320`: badge, menu, brightness, about, task results and notes.
  Picture, game and Nora draw full-bleed.
- Badge: avatar `76x150` (column, info centered to its right) or `216x112`
  (stacked, info centered below); AGENT / QR are compact `108x44` horizontal
  icon/text buttons at body-local `y 236`. Text rows have at least a full font line.
- Menu: seven rows of `216x44` starting at `y 32`, gap `50`; the viewport shows
  four rows at a time.
- Picture: full `240x320`, white canvas, image mapped from flash.
- Nora: full `240x320` white canvas; name only (`Nora`), a native `128x132`
  animated portrait centered below, 20 px subtitles and necessary small prompts.
  No system bar, card border, level meters or long-return reminder. Real long-OK
  navigation remains active, and pairing consent fits the subtitle window.

## Interaction

UP/DOWN move the AGENT/QR highlight on the badge; OK enters the highlighted one
(AGENT → assistant, QR → picture). OK returns from the picture page. Long-OK
(2 s) on the badge opens the hidden menu; on any other page it returns to the
badge. Menu UP/DOWN moves the row highlight; OK opens the item. On the
assistant, task-results and notes pages, UP/DOWN scrolls text or switches notes
and OK performs the page action.

## Text overflow

Profile fields remain still and centered when their measured text width fits.
Only wider fields slide horizontally back and forth at 28 px/s, with a pause at
each end. Field height is at least the font line height in both avatar layouts;
there is no vertical marquee. Research line breaks become spaces for this
single-line display, without changing stored profile data. Other labels truncate
with dots. Uncovered characters show a glyph-missing placeholder.

## Avatar animation

The optional private character pack swaps pre-rendered frames at fixed position
and opacity: idle blink/breath, target-aware attention and a short nod. Focus
updates retain the avatar object. Uploaded single images stay static; the neutral
public placeholder can blink and carries `TEMP`. No runtime transforms or full-
frame decoding are needed. See [motion integration](passport-motion.md).

## Battery icon

The battery icon fill width tracks the reported percent and changes color:
neon at ≥50%, yellow under 50%, red under 20%. An unread battery shows `--%`
and an empty fill.
