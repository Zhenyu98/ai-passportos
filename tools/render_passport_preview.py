#!/usr/bin/env python3
"""Build and render the actual 240x320 LVGL application; no mock screenshots."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
from PIL import Image
import zxingcpp
from configure_passport import normalize_profile

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir",type=Path,required=True)
    parser.add_argument("--output",type=Path,default=ROOT/"build"/"preview")
    parser.add_argument("--assets",type=Path,default=ROOT/"assets")
    parser.add_argument("--profile",type=Path)
    parser.add_argument("--require-motion-scenes",type=int,default=0)
    parser.add_argument("--compiler",default=os.environ.get("CC","cc"))
    parser.add_argument("--lvgl",type=Path,default=ROOT/"managed_components"/"lvgl__lvgl")
    args=parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    subprocess.run(["cmake","-S",str(ROOT/"tools"/"preview"),"-B",str(args.build_dir),"-G","Ninja",
                    "-DCMAKE_C_COMPILER="+args.compiler,"-DPASSPORT_ASSETS="+str(args.assets.resolve()),
                    "-DLVGL_DIR="+str(args.lvgl.resolve())],check=True)
    subprocess.run(["cmake","--build",str(args.build_dir),"-j","8"],check=True)
    command=[str(args.build_dir/("passport_preview.exe" if os.name=="nt" else "passport_preview")),str(args.output)]
    if args.profile:
        profile=normalize_profile(json.loads(args.profile.read_text(encoding="utf-8")))
        fixture=args.output/"profile.local.txt"
        fixture.write_text("\n".join(profile[k].replace("\n"," ") for k in ("name","title","research","bio"))+"\n",encoding="utf-8")
        command.append(str(fixture))
    # The requested QR preview must come from this execution, never an older UI.
    (args.output/"homepage.rgb565").unlink(missing_ok=True)
    env=os.environ.copy()
    if args.require_motion_scenes:
        env["PASSPORT_PREVIEW_REQUIRE_MOTION_SCENES"]=str(args.require_motion_scenes)
    subprocess.run(command,check=True,timeout=60,env=env)
    if not (args.output/"homepage.rgb565").is_file():
        raise RuntimeError("Current UI fixture did not render the picture/QR acceptance case")
    for raw in args.output.glob("*.rgb565"):
        Image.frombytes("RGB",(240,320),raw.read_bytes(),"raw","BGR;16").save(raw.with_suffix(".png"))
    decoded=zxingcpp.read_barcode(Image.open(args.output/"homepage.png"))
    if decoded is None or decoded.text!="https://example.org/a":
        raise RuntimeError("Actual rendered QR did not decode exactly")
    print("Rendered UI, compiled glyph coverage, repeated transitions and exact QR decode: PASS")
if __name__=="__main__":
    main()
