#!/usr/bin/env python3
"""Turn a Codex pet (hatch-pet v1/v2 atlas) into the badge's private motion pack.

The pet's idle, review and waving rows become Passport's idle, focus and nod
states: 18 RGBA 76x150 frames written to a Git-ignored pack directory. With
--install the pack is also compiled into build/passport-assets, seeded from the
public assets, and a private/passport-build.json is created when none exists.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path

from PIL import Image

from build_passport_assets import ROOT, validate_destination
from build_passport_motion import HEIGHT, STATES, WIDTH, generate_motion

CELL_W, CELL_H, COLUMNS = 192, 208, 8
ATLAS_ROWS = {"v1": 9, "v2": 11}
PET_ROWS = {"idle": 0, "running-right": 1, "running-left": 2, "waving": 3, "jumping": 4,
            "failed": 5, "waiting": 6, "running": 7, "review": 8}
# Used columns per hatch-pet row (references/animation-rows.md).
PET_FRAMES = {"idle": 6, "running-right": 8, "running-left": 8, "waving": 4, "jumping": 5,
              "failed": 8, "waiting": 6, "running": 6, "review": 6}
DEFAULT_MAP = {"idle": "idle", "focus": "review", "nod": "waving"}
MARGIN = 2  # build_passport_motion rejects frames touching the outer 2 px.


def six_columns(count: int) -> list[int]:
    """Six frame indices: the row itself, or a ping-pong loop for shorter rows."""
    if count >= 6:
        return list(range(6))
    loop = list(range(count)) + list(range(count - 2, 0, -1))
    return [loop[i % len(loop)] for i in range(6)]


def load_pet(pet_dir: Path) -> tuple[dict, Image.Image]:
    meta = json.loads((pet_dir / "pet.json").read_text(encoding="utf-8"))
    sheet = (pet_dir / meta.get("spritesheetPath", "spritesheet.webp")).resolve()
    if not sheet.is_relative_to(pet_dir.resolve()):
        raise ValueError("Spritesheet must stay inside the pet directory")
    with Image.open(sheet) as opened:
        atlas = opened.convert("RGBA")
    if atlas.width != CELL_W * COLUMNS or atlas.height not in {CELL_H * rows for rows in ATLAS_ROWS.values()}:
        raise ValueError(f"Unexpected Codex pet atlas size {atlas.size}")
    return meta, atlas


def extract_frames(atlas: Image.Image, mapping: dict[str, str]) -> dict[str, list[Image.Image]]:
    frames = {}
    for state in STATES:
        row = mapping[state]
        if row not in PET_ROWS:
            raise ValueError(f"Unknown pet row: {row}")
        cells = []
        for column in six_columns(PET_FRAMES[row]):
            box = (column * CELL_W, PET_ROWS[row] * CELL_H, (column + 1) * CELL_W, (PET_ROWS[row] + 1) * CELL_H)
            cell = atlas.crop(box)
            if not cell.getchannel("A").getbbox():
                raise ValueError(f"Empty pet cell: {row}/{column}")
            cells.append(cell)
        frames[state] = cells
    return frames


def fit_frames(frames: dict[str, list[Image.Image]]) -> dict[str, list[Image.Image]]:
    """One shared crop, scale and feet baseline for all 18 frames, so the
    character never pops in size or slides between states."""
    boxes = [cell.getchannel("A").getbbox() for cells in frames.values() for cell in cells]
    left, top = min(b[0] for b in boxes), min(b[1] for b in boxes)
    right, bottom = max(b[2] for b in boxes), max(b[3] for b in boxes)
    scale = min((WIDTH - 2 * MARGIN - 2) / (right - left), (HEIGHT - 2 * MARGIN - 2) / (bottom - top))
    width, height = max(1, int((right - left) * scale)), max(1, int((bottom - top) * scale))
    x, y = (WIDTH - width) // 2, HEIGHT - MARGIN - 1 - height
    fitted = {}
    for state, cells in frames.items():
        fitted[state] = []
        for cell in cells:
            # Premultiplied resize keeps edge colours clean against transparency.
            scaled = cell.crop((left, top, right, bottom)).convert("RGBa").resize(
                (width, height), Image.Resampling.LANCZOS).convert("RGBA")
            frame = Image.new("RGBA", (WIDTH, HEIGHT))
            frame.alpha_composite(scaled, (x, y))
            fitted[state].append(frame)
    return fitted


def write_pack(fitted: dict[str, list[Image.Image]], output: Path, *, meta: dict,
               atlas_sha: str, mapping: dict[str, str]) -> None:
    validate_destination(output, True)
    for state, cells in fitted.items():
        directory = output / "frames" / state
        directory.mkdir(parents=True, exist_ok=True)
        for old in directory.glob("*.png"):
            old.unlink()
        for index, frame in enumerate(cells):
            frame.save(directory / f"{index:02d}.png")
    manifest = {"version": "codex-pet-1", "dimensions": [WIDTH, HEIGHT], "layout": "6 columns x 3 rows",
                "rows": list(STATES), "source": {"pet_id": meta.get("id"), "display_name": meta.get("displayName"),
                "spritesheet_sha256": atlas_sha, "row_map": mapping}}
    (output / "animation-manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                                                    encoding="utf-8", newline="\n")


def install(pack: Path, assets: Path) -> dict:
    """Compile the pack into a personal asset tree and point the build at it."""
    validate_destination(assets, True)
    if not (assets / "passport" / "passport_glyphs.c").is_file():
        shutil.copytree(ROOT / "assets", assets, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns("private", "README*"))
    manifest = generate_motion(pack, assets)
    settings = ROOT / "private" / "passport-build.json"
    if not settings.exists():
        settings.parent.mkdir(exist_ok=True)
        relative = assets.resolve().relative_to(ROOT).as_posix() if assets.resolve().is_relative_to(ROOT) else str(assets)
        settings.write_text(json.dumps({"asset_dir": relative, "required_motion_scenes": 1}, indent=2) + "\n",
                            encoding="utf-8", newline="\n")
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pet", type=Path, required=True, help="Codex pet folder, e.g. ~/.codex/pets/<id>")
    parser.add_argument("--output", type=Path, default=ROOT / "assets/private/passport-motion")
    parser.add_argument("--focus-row", default=DEFAULT_MAP["focus"], choices=sorted(PET_ROWS))
    parser.add_argument("--nod-row", default=DEFAULT_MAP["nod"], choices=sorted(PET_ROWS))
    parser.add_argument("--install", type=Path, nargs="?", const=ROOT / "build/passport-assets",
                        help="Also compile into this personal asset directory (default build/passport-assets)")
    args = parser.parse_args()
    pet = args.pet.expanduser()
    mapping = {"idle": "idle", "focus": args.focus_row, "nod": args.nod_row}
    meta, atlas = load_pet(pet)
    sheet = pet / meta.get("spritesheetPath", "spritesheet.webp")
    write_pack(fit_frames(extract_frames(atlas, mapping)), args.output, meta=meta,
               atlas_sha=hashlib.sha256(sheet.read_bytes()).hexdigest(), mapping=mapping)
    print(f"Motion pack: {args.output} ({meta.get('displayName') or meta.get('id')}, rows {mapping})")
    if args.install:
        manifest = install(args.output, args.install)
        print(f"Installed: {args.install} ({manifest['frame_count']} frames, {manifest['pixel_bytes']} Flash bytes)")


if __name__ == "__main__":
    main()
