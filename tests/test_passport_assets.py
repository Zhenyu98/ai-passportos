import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
from build_passport_assets import collect_glyphs, generate_assets, validate_destination

class AssetTest(unittest.TestCase):
    def test_badge_bold_font_is_real_weight_700_and_covers_chinese_inventory(self):
        manifest=json.loads((ROOT/'assets/passport/bold-font-manifest.json').read_text())
        self.assertEqual((manifest['size'],manifest['weight'],manifest['bpp']),(24,700,2))
        self.assertTrue(set(collect_glyphs({}))<=set(manifest['glyphs']))
        source=ROOT/'assets/fonts/passport_font_24_bold.c'
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),manifest['source_sha256'])
    def test_default_fonts_allow_editing_common_chinese_names(self):
        glyphs=set(collect_glyphs({"title":"工牌"},None))
        expected={ord(c) for c in "张三李四王伟陈晓明赵钱孙刘黄周吴郑人工智能体，。"}
        self.assertTrue(expected<=glyphs)
        self.assertGreaterEqual(len(glyphs),6763)
    def test_character_inventory_contains_ascii_ui_and_profile(self):
        glyphs=collect_glyphs({"title":"身份卡"},{"name":"中","bio":"a\nb"})
        self.assertTrue(set(range(32,127))<=set(glyphs))
        self.assertTrue({ord(c) for c in "身份卡中"}<=set(glyphs))
        self.assertNotIn(10,glyphs)
        self.assertEqual(glyphs,sorted(set(glyphs)))
    def test_private_assets_cannot_overwrite_public_sources(self):
        with self.assertRaises(ValueError): validate_destination(ROOT/"assets",True)
        validate_destination(ROOT/"build"/"test-private-assets",True)
    def tools(self):
        keys=["PASSPORT_FONT_SOURCE","PASSPORT_FONT_CONV","PASSPORT_NODE"]
        if not all(os.environ.get(key) for key in keys):
            self.skipTest("Font regeneration tools are not configured")
        return {name:Path(os.environ[key]) for name,key in zip(["font","converter","node"],keys)}
    def test_generation_is_reproducible_and_frames_are_bounded(self):
        tools=self.tools()
        with tempfile.TemporaryDirectory(prefix="passport-assets-test-") as tmp:
            a=Path(tmp)/"one"; b=Path(tmp)/"two"
            first=generate_assets(output=a,**tools)
            second=generate_assets(output=b,**tools)
            self.assertEqual(first,second)
            self.assertEqual(first["avatar"],{"frames":3,"width":64,"height":64,"pixel_bytes":24576,"alpha":False})
            self.assertFalse(first["private"])
            header=(a/"passport"/"passport_strings.h").read_text(encoding="utf-8")
            self.assertIn("PASSPORT_TEXT_NAME_UNSET",header)
            ui=json.loads((ROOT/"assets/passport/ui-text.json").read_text(encoding="utf-8"))
            self.assertIn(ui["name_unset"],header)
            for name,digest in first["files"].items():
                self.assertEqual(hashlib.sha256((a/name).read_bytes()).hexdigest(),digest)
            image_source=(a/"images"/"passport_avatar.c").read_text()
            arrays=re.findall(r"passport_avatar_pixels_\d+\[8192\]\s*=\s*\{(.*?)\};",image_source,re.S)
            self.assertEqual(len(arrays),3)
            self.assertTrue(all(len(re.findall(r"0x[0-9a-f]{2}",data))==8192 for data in arrays))
    def test_missing_glyphs_fail_instead_of_silently_disappearing(self):
        tools=self.tools()
        with tempfile.TemporaryDirectory(prefix="passport-assets-missing-") as tmp:
            with self.assertRaisesRegex(ValueError,"glyph"):
                generate_assets(output=Path(tmp),profile={"name":"\U0010ffff"},**tools)
    def test_public_manifest_matches_committed_sources(self):
        manifest=json.loads((ROOT/"assets"/"passport"/"manifest.json").read_text())
        self.assertFalse(manifest["private"])
        self.assertEqual(manifest["ui_sha256"],hashlib.sha256((ROOT/"assets"/"passport"/"ui-text.json").read_bytes()).hexdigest())
        for name,expected in manifest["files"].items():
            self.assertEqual(hashlib.sha256((ROOT/"assets"/name).read_bytes()).hexdigest(),expected)
    def test_private_generation_is_marked_and_keeps_profile_text_out_of_manifest(self):
        tools=self.tools()
        with tempfile.TemporaryDirectory(prefix="passport-private-assets-test-") as tmp:
            manifest=generate_assets(output=Path(tmp),profile={"name":"Private Example 中"},**tools)
            self.assertTrue(manifest["private"])
            self.assertIn(ord("中"),manifest["glyphs"])
            self.assertNotIn("Private Example",json.dumps(manifest))

if __name__=="__main__": unittest.main()
