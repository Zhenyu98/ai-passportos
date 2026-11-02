from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

def host_bash():
    """Return a bash that can run tools/validate.sh against this checkout.

    On native Windows, `bash` on PATH is usually the WSL launcher in System32.
    WSL cannot run tools/validate.sh through the Windows repository path (and
    may not be installed at all), so prefer Git for Windows bash: that is the
    same shell tools/validate.ps1 discovers for the native static gate. Return
    None when no compatible bash exists so the test skips instead of testing
    the environment.
    """
    configured=os.environ.get("PASSPORT_BASH")
    if configured: return configured
    if os.name!="nt": return shutil.which("bash")
    git=shutil.which("git")
    if git:
        candidate=Path(git).resolve().parent.parent/"bin"/"bash.exe"
        if candidate.is_file(): return str(candidate)
    for candidate in (Path(r"C:\Program Files\Git\bin\bash.exe"),
                      Path(r"C:\Program Files (x86)\Git\bin\bash.exe")):
        if candidate.is_file(): return str(candidate)
    return None

class ValidationHostTest(unittest.TestCase):
    def test_static_gate_executes_the_selected_host_python(self):
        bash=host_bash()
        if not bash: self.skipTest("No bash able to run tools/validate.sh (Git for Windows bash required on Windows)")
        with tempfile.TemporaryDirectory(prefix="passport-host-python-") as directory:
            selected=Path(directory)/"selected-python"
            selected.write_text("#!/bin/sh\necho SELECTED_HOST_PYTHON >&2\nexit 73\n")
            selected.chmod(0o755)
            env=os.environ.copy();env["PASSPORT_HOST_PYTHON"]=str(selected)
            result=subprocess.run([bash,str(ROOT/"tools"/"validate.sh"),"--static"],
                                  env=env,cwd=ROOT,capture_output=True,timeout=30)
            self.assertEqual(result.returncode,73,result.stderr.decode(errors="replace"))
            self.assertIn(b"SELECTED_HOST_PYTHON",result.stderr)
if __name__=="__main__": unittest.main()
