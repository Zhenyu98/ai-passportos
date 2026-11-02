import json
from pathlib import Path
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import import_codex_pet as pet  # noqa: E402
from build_passport_motion import generate_motion  # noqa: E402


def make_pet(directory: Path, rows: int = 11) -> None:
    atlas = Image.new("RGBA", (pet.CELL_W * pet.COLUMNS, pet.CELL_H * rows))
    draw = ImageDraw.Draw(atlas)
    for name, row in pet.PET_ROWS.items():
        for column in range(pet.PET_FRAMES[name]):
            x, y = column * pet.CELL_W, row * pet.CELL_H
            # Grounded body with a per-frame head bob, as hatch-pet rows keep feet planted.
            draw.rectangle((x + 60, y + 90, x + 130, y + 196), fill=(30, 30, 40, 255))
            draw.ellipse((x + 64, y + 30 + column % 3, x + 126, y + 92 + column % 3), fill=(240, 200, 170, 255))
    atlas.save(directory / "spritesheet.webp", lossless=True)
    (directory / "pet.json").write_text(json.dumps({"id": "demo", "displayName": "Demo",
                                                    "spritesheetPath": "spritesheet.webp"}), encoding="utf-8")


class CodexPetImport(unittest.TestCase):
    def test_short_rows_ping_pong_to_six_frames(self):
        self.assertEqual(pet.six_columns(4), [0, 1, 2, 3, 2, 1])
        self.assertEqual(pet.six_columns(6), [0, 1, 2, 3, 4, 5])
        self.assertEqual(pet.six_columns(8), [0, 1, 2, 3, 4, 5])

    def test_v2_and_v1_atlases_become_a_packable_motion_pack(self):
        for rows in (11, 9):
            with tempfile.TemporaryDirectory() as temp:
                temp = Path(temp)
                (temp / "pet").mkdir()
                make_pet(temp / "pet", rows)
                meta, atlas = pet.load_pet(temp / "pet")
                fitted = pet.fit_frames(pet.extract_frames(atlas, pet.DEFAULT_MAP))
                pet.write_pack(fitted, temp / "pack", meta=meta, atlas_sha="0" * 64, mapping=pet.DEFAULT_MAP)
                manifest = generate_motion(temp / "pack", temp / "assets")
                self.assertEqual(manifest["frame_count"], 18)
                source = json.loads((temp / "pack/animation-manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(source["source"]["row_map"]["nod"], "waving")

    def test_rejects_unexpected_atlas_geometry(self):
        with tempfile.TemporaryDirectory() as temp:
            temp = Path(temp)
            Image.new("RGBA", (1000, 1000)).save(temp / "spritesheet.webp", lossless=True)
            (temp / "pet.json").write_text("{}", encoding="utf-8")
            with self.assertRaises(ValueError):
                pet.load_pet(temp)


if __name__ == "__main__":
    unittest.main()
