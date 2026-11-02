import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("prepare_muse",ROOT/"tools/prepare_passport_muse.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class PrepareMuse(unittest.TestCase):
    def test_patch_refuses_ambiguous_or_missing_anchor(self):
        self.assertEqual(module.replace_once("a START b","START","END"),"a END b")
        for source in ("none","START START"):
            with self.assertRaises(ValueError): module.replace_once(source,"START","END")

    def test_destination_must_be_ignored_build_child(self):
        with self.assertRaises(ValueError): module.validate_output(ROOT)
        with self.assertRaises(ValueError): module.validate_output(ROOT/"main")
        module.validate_output(ROOT/"build/muse-source")

if __name__=="__main__": unittest.main()
