from pathlib import Path
import importlib.util
import json
import tempfile
import unittest
import sys
from PIL import Image,ImageDraw

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
(ROOT/'build').mkdir(exist_ok=True)  # Git-ignored; absent in a fresh clone
spec=importlib.util.spec_from_file_location('agent_avatar',ROOT/'tools/build_passport_agent_avatar.py')
pack=importlib.util.module_from_spec(spec);spec.loader.exec_module(pack)

class AgentAvatarTests(unittest.TestCase):
    def test_pack_has_eight_separate_flash_frames(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            root=Path(folder);sheet=Image.new('RGBA',(256,128))
            draw=ImageDraw.Draw(sheet)
            for index in range(8):
                x=index%4*64;y=index//4*64
                draw.ellipse((x+12,y+8,x+52,y+62),fill=(index*25,80,120,255))
            sheet.save(root/'sheet.png')
            manifest=pack.generate(root/'sheet.png',root/'out')
            self.assertEqual(manifest['frame_count'],8)
            self.assertEqual(manifest['pixel_bytes'],8*128*132*3)
            source=(root/'out/images/passport_agent_avatar.c').read_text()
            self.assertIn('"Nora"',source)
            self.assertEqual(source.count('.data_size='),8)
            self.assertIn('LV_COLOR_FORMAT_RGB565A8',source)
            self.assertEqual(json.loads((root/'out/passport/agent-avatar-manifest.json').read_text()),manifest)
    def test_opaque_screenshot_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
            root=Path(folder);Image.new('RGBA',(256,128),'white').save(root/'sheet.png')
            with self.assertRaises(ValueError):pack.generate(root/'sheet.png',root/'out')

if __name__=='__main__':unittest.main()
