import json
import re
from pathlib import Path
import sys
import tempfile
import unittest

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_passport_motion import generate_motion


class MotionAssetsTest(unittest.TestCase):
    def pack(self, root):
        (root / "animation-manifest.json").write_text(json.dumps({
            "version": "design-preview-1", "dimensions": [76, 150],
            "rows": ["idle", "focus", "nod"]}), encoding="utf-8")
        for row, state in enumerate(("idle", "focus", "nod")):
            directory = root / "frames" / state
            directory.mkdir(parents=True)
            for frame in range(6):
                image = Image.new("RGBA", (76, 150))
                ImageDraw.Draw(image).rectangle((12, 20 + frame, 60, 145), fill=(40 + row * 20, 80, 100 + frame, 255))
                image.save(directory / f"{frame:02d}.png")

    def test_pack_is_deterministic_and_payload_is_planar_rgb565a8(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); self.pack(root)
            a, b = root / "a", root / "b"
            first = generate_motion(root, a)
            second = generate_motion(root, b)
            self.assertEqual(first, second)
            self.assertEqual(first["frame_count"], 18)
            self.assertEqual(first["frame_bytes"], 34200)
            self.assertTrue(first["private"])
            self.assertEqual((a / "images/passport_motion.c").read_bytes(),
                             (b / "images/passport_motion.c").read_bytes())
            self.assertNotIn(str(root), json.dumps(first))
            source = (a / "images/passport_motion.c").read_text(encoding="utf-8")
            self.assertIn("LV_COLOR_FORMAT_RGB565A8", source)
            self.assertIn("passport_motion_count = 18", source)
            first_array = re.search(r"motion_pixels_0\[34200\] = \{(.*?)\};", source, re.S).group(1)
            packed = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", first_array))
            offset = 20 * 76 + 12
            # Independent known pixel: RGB (40,80,100) -> 0x2a8c, little endian.
            self.assertEqual(packed[offset * 2:offset * 2 + 2], b"\x8c\x2a")
            self.assertEqual(packed[:2], b"\x00\x00")
            self.assertEqual(packed[22800 + offset], 255)
            self.assertEqual(packed[22800], 0)
            self.assertEqual(packed[22800:].count(255), 49 * 126)

    def test_rejects_clipped_empty_wrong_size_and_baseline_drift(self):
        for invalid in ("clipped", "empty", "size", "baseline"):
            with self.subTest(invalid=invalid), tempfile.TemporaryDirectory() as temp:
                root = Path(temp); self.pack(root)
                image = Image.open(root / "frames/nod/02.png").convert("RGBA")
                if invalid == "clipped":
                    image.putpixel((0, 50), (255, 0, 0, 255))
                elif invalid == "empty":
                    image = Image.new("RGBA", (76, 150))
                elif invalid == "size":
                    image = image.resize((77, 150))
                else:
                    image = Image.new("RGBA", (76, 150))
                    ImageDraw.Draw(image).rectangle((12, 20, 60, 130), fill="white")
                image.save(root / "frames/nod/02.png")
                with self.assertRaises(ValueError):
                    generate_motion(root, root / "output")
                self.assertFalse((root / "output/images/passport_motion.c").exists())

    def test_missing_frame_and_public_output_fail_without_replacing_assets(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); self.pack(root)
            with self.assertRaises(ValueError):
                generate_motion(root, ROOT / "assets")
            (root / "frames/idle/05.png").unlink()
            with self.assertRaises(ValueError):
                generate_motion(root, root / "output")

    def scene_pack(self, root):
        specs = {
            "night": {"sprite": [88,112], "stage": [216,112], "offset": [10,0],
                      "durations": [1800,500,100,120,600,1500]},
            "cat": {"sprite": [76,112], "stage": [76,150], "offset": [0,19],
                    "durations": [800,180,220,160,180,770]}}
        (root / "animation-manifest.json").write_text(json.dumps(specs), encoding="utf-8")
        for state, spec in specs.items():
            directory = root / "frames" / state; directory.mkdir(parents=True)
            for i in range(6):
                image = Image.new("RGBA", tuple(spec["sprite"]))
                ImageDraw.Draw(image).rectangle((8+i,8,60,100), fill=(50+i,80,100,255))
                image.save(directory / f"{i:02d}.png")
                if state == "night":
                    stage = Image.new("RGB", (216,112), (10,28,51))
                    stage.paste(image, (10,0), image)
                    stage.save(directory / f"scene-{i:02d}.png")

    def test_two_scene_pack_keeps_default_interactions_and_distinct_layouts(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); self.pack(root)
            scenes=root/"scenes"; scenes.mkdir(); self.scene_pack(scenes)
            result=generate_motion(root,root/"output",scene_pack=scenes)
            self.assertEqual(result["frame_count"],18)
            self.assertEqual(result["scene_count"],3)
            self.assertEqual([s["id"] for s in result["scenes"]],[1,2,3])
            self.assertEqual([s["frame_count"] for s in result["scenes"]],[18,6,6])
            source=(root/"output/images/passport_motion.c").read_text(encoding="utf-8")
            self.assertIn("LV_COLOR_FORMAT_RGB565,",source)
            self.assertIn(".w=216,.h=112",source)
            self.assertIn(".w=76,.h=150",source)
            self.assertIn("passport_motion_scene_count = 3",source)

    def test_public_showcase_keeps_only_selected_scenes(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp); self.pack(root)
            scenes=root/"scenes"; scenes.mkdir(); self.scene_pack(scenes)
            data=json.loads((scenes/"animation-manifest.json").read_text()); del data["cat"]
            (scenes/"animation-manifest.json").write_text(json.dumps(data))  # night-only pack
            result=generate_motion(root,root/"output",scene_pack=scenes,scene_keys=("night",),public=True)
            self.assertFalse(result["private"])
            self.assertEqual([s["id"] for s in result["scenes"]],[1,2])
            source=(root/"output/images/passport_motion.c").read_text(encoding="utf-8")
            self.assertTrue(source.startswith("/* Public showcase motion"))
            self.assertIn("passport_motion_scene_count = 2",source)

    def test_committed_public_motion_is_the_two_scene_showcase(self):
        source=(ROOT/"assets/images/passport_motion.c").read_text(encoding="utf-8")
        self.assertTrue(source.startswith("/* Public showcase motion"))
        self.assertIn("passport_motion_scene_count = 2",source)
        self.assertNotIn("撸猫",source)

    def test_invalid_scene_timing_does_not_replace_usable_motion(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);self.pack(root)
            scenes=root/"scenes";scenes.mkdir();self.scene_pack(scenes)
            output=root/"output";generate_motion(root,output)
            before=(output/"images/passport_motion.c").read_bytes()
            path=scenes/"animation-manifest.json";data=json.loads(path.read_text())
            data["cat"]["durations"][2]=0;path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):generate_motion(root,output,scene_pack=scenes)
            self.assertEqual((output/"images/passport_motion.c").read_bytes(),before)


if __name__ == "__main__":
    unittest.main()
