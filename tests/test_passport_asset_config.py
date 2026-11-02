import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class PersonalBuildConfigTest(unittest.TestCase):
    def run_config(self, root, local=True):
        cmake=os.environ.get("PASSPORT_CMAKE") or shutil.which("cmake")
        if not cmake:
            candidate=Path("C:/Espressif/tools-v5.5.3/tools/cmake/3.30.2/bin/cmake.exe")
            if candidate.is_file():cmake=str(candidate)
        if not cmake:self.skipTest("CMake unavailable")
        script=root/"probe.cmake"
        script.write_text('include("'+(ROOT/'tools/cmake/passport_assets.cmake').as_posix()+'")\n'
                          'file(WRITE "'+(root/'selected.txt').as_posix()+'" "${PASSPORT_ASSETS}")\n')
        return subprocess.run([cmake,'-DPASSPORT_SOURCE_ROOT='+root.as_posix(),
                               '-DPASSPORT_USE_LOCAL_ASSETS='+('ON' if local else 'OFF'),
                               '-P',str(script)],capture_output=True,text=True)
    def setup_root(self, root):
        (root/'private').mkdir();(root/'assets/images').mkdir(parents=True)
        (root/'assets/images/passport_motion.c').write_text('public empty set')
        target=root/'build/personal';(target/'images').mkdir(parents=True);(target/'passport').mkdir()
        source=target/'images/passport_motion.c';source.write_text('three populated scenes')
        (target/'passport/motion-manifest.json').write_text(json.dumps({'frame_count':18,'scene_count':3,
            'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()}))
        (root/'private/passport-build.json').write_text(json.dumps({'asset_dir':'build/personal','required_motion_scenes':3}))
        return target
    def test_personal_build_selects_animation_assets_without_manual_override(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);target=self.setup_root(root);result=self.run_config(root)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(Path((root/'selected.txt').read_text()),target)
    def test_missing_or_tampered_motion_fails_instead_of_silent_public_fallback(self):
        for damage in ('missing','empty','hash'):
            with self.subTest(damage=damage),tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);target=self.setup_root(root)
                if damage=='missing':(target/'images/passport_motion.c').unlink()
                elif damage=='hash':(target/'images/passport_motion.c').write_text('empty replacement')
                else:
                    p=target/'passport/motion-manifest.json';m=json.loads(p.read_text());m['scene_count']=0;p.write_text(json.dumps(m))
                result=self.run_config(root);self.assertNotEqual(result.returncode,0)
    def test_explicit_public_build_works_without_private_artwork(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.setup_root(root);result=self.run_config(root,local=False)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(Path((root/'selected.txt').read_text()),root/'assets')

    def test_required_nora_cannot_silently_fall_back_to_badge_character(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);self.setup_root(root)
            p=root/'private/passport-build.json';options=json.loads(p.read_text());options['required_agent_avatar']=True
            p.write_text(json.dumps(options));result=self.run_config(root)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('Required Nora avatar missing',result.stderr)
            self.assertEqual(self.run_config(root,local=False).returncode,0)

    def test_nora_source_checksum_is_verified(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);target=self.setup_root(root)
            source=target/'images/passport_agent_avatar.c';source.write_text('eight Nora frames')
            manifest={'frame_count':8,'width':128,'height':132,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
            (target/'passport/agent-avatar-manifest.json').write_text(json.dumps(manifest))
            self.assertEqual(self.run_config(root).returncode,0)
            source.write_text('wrong character')
            result=self.run_config(root);self.assertNotEqual(result.returncode,0)
            self.assertIn('Muse character checksum mismatch',result.stderr)

    def test_incremental_build_rejects_a_regenerated_pack_that_lost_scenes(self):
        cmake=shutil.which('cmake') or 'C:/Espressif/tools-v5.5.3/tools/cmake/3.30.2/bin/cmake.exe'
        ninja=shutil.which('ninja') or 'C:/Espressif/tools-v5.5.3/tools/ninja/1.12.1/ninja.exe'
        if not Path(cmake).is_file() or not Path(ninja).is_file():self.skipTest('CMake/Ninja unavailable')
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);target=self.setup_root(root);build=root/'cmake-build'
            (root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.20)\nproject(probe NONE)\n'
                'include("'+(ROOT/'tools/cmake/passport_assets.cmake').as_posix()+'")\n'
                'add_custom_target(probe ALL COMMAND "${CMAKE_COMMAND}" -E echo "validated")\n')
            configure=subprocess.run([cmake,'-S',str(root),'-B',str(build),'-G','Ninja',
                '-DCMAKE_MAKE_PROGRAM='+ninja,'-DPASSPORT_SOURCE_ROOT='+root.as_posix()],capture_output=True,text=True)
            self.assertEqual(configure.returncode,0,configure.stderr)
            first=subprocess.run([cmake,'--build',str(build)],capture_output=True,text=True)
            self.assertEqual(first.returncode,0,first.stderr)
            manifest=target/'passport/motion-manifest.json';m=json.loads(manifest.read_text());m['scene_count']=0
            manifest.write_text(json.dumps(m))
            second=subprocess.run([cmake,'--build',str(build)],capture_output=True,text=True)
            self.assertNotEqual(second.returncode,0,'Incremental build silently accepted a pack without scenes')

if __name__=='__main__':unittest.main()
