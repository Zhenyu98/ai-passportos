#!/usr/bin/env python3
"""Compile focused application tests using a native host C compiler."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {
    "speech": ["tests/test_passport_speech.c", "main/passport_speech.c"],
    "speech_http": ["tests/test_passport_speech_http.c", "main/passport_speech.c", "main/passport_speech_http.c"],
    "config_auth": ["tests/test_passport_config_auth.c", "main/passport_config_auth.c"],
    "agent_runtime": ["tests/test_passport_agent_runtime.c", "main/passport_agent_model.c"],
    "muse_tools": ["tests/test_passport_muse_tools.c", "main/passport_muse_tools.c", "main/passport_agent_model.c"],
    "agent": ["tests/test_passport_agent.c", "main/passport_agent_model.c"],
    "animation": ["tests/test_passport_animation.c", "main/passport_animation.c"],
    "model": ["tests/test_passport_model.c", "main/passport_model.c"],
    "game": ["tests/test_passport_game.c", "main/passport_game.c"],
    "profile": ["tests/test_passport_profile.c", "main/passport_profile.c"],
    "store": ["tests/test_passport_store.c", "main/passport_profile.c", "main/passport_store.c"],
    "nvs": ["tests/test_passport_nvs.c", "main/passport_profile.c", "main/passport_store.c", "main/passport_nvs.c"],
    "protocol": ["tests/test_passport_protocol.c", "main/passport_profile.c", "main/passport_protocol.c", "main/passport_json.c"],
    "usb": ["tests/test_passport_usb.c", "main/passport_profile.c", "main/passport_protocol.c", "main/passport_json.c"],
    "qr": ["tests/test_passport_qr.c", "main/passport_profile.c", "main/passport_qr.c"],
    "screen": ["tests/test_passport_screen.c", "main/passport_screen.c"],
    "runtime": ["tests/test_passport_runtime.c", "main/passport_model.c", "main/passport_profile.c", "main/passport_store.c", "main/passport_screen.c"],
    "settings": ["tests/test_passport_settings.c", "main/passport_settings.c", "main/passport_profile.c", "main/passport_json.c"],
    "media": ["tests/test_passport_media.c", "main/passport_scene.c"],
}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("groups", nargs="*", default=["all"])
    args = parser.parse_args()
    names = list(GROUPS) if "all" in args.groups else args.groups
    unknown = set(names) - GROUPS.keys()
    if unknown:
        parser.error("Unknown groups: " + ", ".join(sorted(unknown)))
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("clang")
    if not compiler:
        candidate = Path("C:/Espressif/host-tools/llvm-mingw-20261006-ucrt-x86_64/bin/clang.exe")
        compiler = str(candidate) if candidate.is_file() else None
    if not compiler:
        parser.error("Set CC to a native C compiler executable")
    output = ROOT / "build" / "host-tests"
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
    for name in names:
        binary = output / ("passport_" + name + (".exe" if os.name == "nt" else ""))
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Itests/passport_stubs", "-Imain", *GROUPS[name], "-o", str(binary)]
        if name=="media":
            command.insert(1,"-Itests/passport_media_stubs")
        if name in {"agent_runtime","speech_http"}:
            command[1:1]=["-Itests/agent_stubs","-Icomponents/bsp/include"]
        if name=="speech_http" and os.name!="nt":command[1:1]=["-D_POSIX_C_SOURCE=200809L","-pthread"]
        if name in {"protocol","usb","settings","muse_tools","speech_http"}:
            idf = Path(os.environ.get("IDF_PATH", ""))
            cjson = idf / "components" / "json" / "cJSON"
            if not (cjson / "cJSON.c").is_file():
                cjson=ROOT/"build"/"host-deps"/"cjson"
            if not (cjson / "cJSON.c").is_file():
                parser.error("Run python tools/prepare_passport_host.py for hash-pinned host sources")
            command += ["-DCJSON_HIDE_SYMBOLS", "-I" + str(cjson), str(cjson / "cJSON.c"), "-lm"]
        if name == "runtime":
            command += ["-Icomponents/bsp/include", "-Iassets/passport", "-DPASSPORT_HOST_TEST"]
        if name == "qr":
            lvgl=ROOT/"managed_components"/"lvgl__lvgl"
            if not (lvgl/"src"/"libs"/"qrcode"/"qrcodegen.c").is_file():
                lvgl=ROOT/"build"/"host-deps"/"lvgl"
            encoder=lvgl/"src"/"libs"/"qrcode"
            if not (encoder/"qrcodegen.c").is_file():
                parser.error("Build the pinned LVGL managed component before QR tests")
            command += ["-DLV_CONF_SKIP=1","-DLV_USE_QRCODE=1","-DLV_USE_LOG=0",
                        "-I"+str(lvgl),"-I"+str(encoder),str(encoder/"qrcodegen.c")]
        print("Build " + name, flush=True)
        result = subprocess.run(command, cwd=ROOT, env=env)
        if result.returncode:
            return result.returncode
        try:
            result = subprocess.run([str(binary)], cwd=ROOT, env=env, timeout=30)
        except subprocess.TimeoutExpired:
            print("Test timed out: " + name, file=sys.stderr)
            return 1
        if result.returncode:
            return result.returncode
        if name == "qr":
            from PIL import Image
            import zxingcpp
            prefix="https://example.org/"
            fixtures={"short":prefix+"a","replaced":prefix+"b","maximum":prefix+"a"*(128-len(prefix))}
            for fixture,expected in fixtures.items():
                with Image.open(output/("qr-"+fixture+".pgm")) as picture:
                    decoded=zxingcpp.read_barcode(picture)
                if decoded is None or decoded.text!=expected:
                    print("Independent QR decode failed: "+fixture,file=sys.stderr)
                    return 1
            print("Independent QR decoding: PASS (3 exact URLs)")
    print(f"Passport host groups: PASS ({len(names)})")
    return 0

if __name__ == "__main__":
    sys.exit(main())
