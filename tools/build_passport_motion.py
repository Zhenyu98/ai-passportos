#!/usr/bin/env python3
"""Pack approved local motion frames as Flash-resident LVGL RGB565A8 assets."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image
from build_passport_assets import validate_destination

STATES = ("idle", "focus", "nod")
WIDTH, HEIGHT = 76, 150


SCENES = (("night",2,"外滩看夜景 · 动态"),("cat",3,"撸猫 · 动态"))


def _scene_data(pack: Path | None, keys=("night","cat")) -> list[dict]:
    if pack is None:
        return []
    specs=json.loads((pack/"animation-manifest.json").read_text(encoding="utf-8"))
    result=[]
    for state,scene_id,label in (scene for scene in SCENES if scene[0] in keys):
        spec=specs[state]; stage=tuple(spec["stage"]);sprite=tuple(spec["sprite"]);offset=tuple(spec["offset"])
        durations=spec["durations"]
        expected=(216,112) if state=="night" else (76,150)
        if (stage!=expected or len(sprite)!=2 or len(offset)!=2 or
            any(type(v) is not int for v in (*sprite,*offset)) or min(*sprite)<=0 or min(*offset)<0 or
            offset[0]+sprite[0]>stage[0] or offset[1]+sprite[1]>stage[1] or
            len(durations)!=6 or any(type(v) is not int or not 1<=v<=10000 for v in durations)):
            raise ValueError("Invalid scene layout or timing")
        directory=pack/"frames"/state
        frames=[]
        for n in range(6):
            with Image.open(directory/f"{n:02d}.png") as opened:
                if opened.mode!="RGBA" or opened.size!=sprite:
                    raise ValueError("Invalid scene sprite")
                foreground=opened.copy()
            if not foreground.getchannel("A").getbbox():
                raise ValueError("Empty scene sprite")
            if state=="night":
                with Image.open(directory/f"scene-{n:02d}.png") as opened:
                    if opened.size!=stage:raise ValueError("Invalid scene stage")
                    frame=opened.convert("RGBA")
            else:
                frame=Image.new("RGBA",stage);frame.alpha_composite(foreground,offset)
            frames.append(frame)
        result.append({"id":scene_id,"key":state,"name":label,"durations":durations,
                       "format":"RGB565" if state=="night" else "RGB565A8","frames":frames})
    return result


def _pixels(image: Image.Image, alpha: bool) -> bytes:
    pixels=bytearray()
    for red,green,blue in image.convert("RGB").getdata():
        value=((red>>3)<<11)|((green>>2)<<5)|(blue>>3)
        pixels.extend((value&255,value>>8))
    if alpha:pixels.extend(image.getchannel("A").tobytes())
    return bytes(pixels)


def generate_motion(pack: Path, output: Path, *, scene_pack: Path | None=None,
                    scene_keys=("night","cat"), public: bool=False) -> dict:
    """public=True writes the committed showcase (assets/) instead of personal art."""
    validate_destination(output, not public)
    request = json.loads((pack / "animation-manifest.json").read_text(encoding="utf-8"))
    if request.get("dimensions") != [WIDTH, HEIGHT] or request.get("rows") != list(STATES):
        raise ValueError("Expected 76x150 idle/focus/nod animation pack")
    payloads, records, baselines = [], [], []
    for state in STATES:
        directory = pack / "frames" / state
        if {p.name for p in directory.glob("*.png")} != {f"{i:02d}.png" for i in range(6)}:
            raise ValueError(f"Expected exactly six frames for {state}")
        for frame in range(6):
            path = directory / f"{frame:02d}.png"
            with Image.open(path) as opened:
                if opened.mode != "RGBA" or opened.size != (WIDTH, HEIGHT):
                    raise ValueError(f"Invalid RGBA frame size: {state}/{frame}")
                image = opened.copy()
            alpha = image.getchannel("A")
            bounds = alpha.getbbox()
            if (bounds is None or alpha.getextrema() != (0, 255) or
                    bounds[0] < 2 or bounds[1] < 2 or bounds[2] > WIDTH-2 or bounds[3] > HEIGHT-2):
                raise ValueError(f"Empty or clipped frame: {state}/{frame}")
            baselines.append(bounds[3])
            pixels = bytearray()
            for red, green, blue in image.convert("RGB").getdata():
                value = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
                pixels.extend((value & 255, value >> 8))
            pixels.extend(alpha.tobytes())
            payloads.append(bytes(pixels))
            records.append({"state": state, "frame": frame,
                            "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    if max(baselines)-min(baselines)>1:
        raise ValueError("Animation foot baseline drift")
    scenes=_scene_data(scene_pack,scene_keys)
    # Validate everything before replacing a previously usable generated source.
    all_images=[("RGB565A8",WIDTH,HEIGHT,p) for p in payloads]
    for scene in scenes:
        scene["payloads"]=[_pixels(im,scene["format"]=="RGB565A8") for im in scene["frames"]]
        all_images.extend((scene["format"],im.width,im.height,p) for im,p in zip(scene["frames"],scene["payloads"]))
    unique=list(dict.fromkeys(all_images));indices={image:index for index,image in enumerate(unique)}
    lines = ['/* Public showcase motion; pre-rendered, no runtime image decoder allocation. */' if public else
             '/* Private, pre-rendered motion; no runtime image decoder allocation. */',
             '#include "passport_assets.h"']
    for index, (_,_,_,payload) in enumerate(unique):
        lines.append(f"static const uint8_t motion_pixels_{index}[{len(payload)}] = {{")
        lines.extend("    " + ",".join(f"0x{x:02x}" for x in payload[at:at+24]) + ","
                     for at in range(0, len(payload), 24))
        lines.append("};")
    lines.append("const lv_image_dsc_t passport_motion_frames[] = {")
    for payload in payloads:
        lines.append("    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8,"
                     f".w={WIDTH},.h={HEIGHT},.stride={WIDTH*2}" + "},"
                     f".data_size={len(payload)},.data=motion_pixels_{indices[('RGB565A8',WIDTH,HEIGHT,payload)]}" + "},")
    lines.extend(["};", "const size_t passport_motion_count = 18;"])
    for scene in scenes:
        key=scene["key"];lines.append(f"static const lv_image_dsc_t scene_{key}_frames[] = {{")
        for im,payload in zip(scene["frames"],scene["payloads"]):
            cf=scene["format"];index=indices[(cf,im.width,im.height,payload)]
            lines.append("    {.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_"+cf+","+
                         f".w={im.width},.h={im.height},.stride={im.width*2}"+"},"+
                         f".data_size={len(payload)},.data=motion_pixels_{index}"+"},")
        lines.append("};")
        lines.append(f"static const uint16_t scene_{key}_durations[] = {{"+",".join(map(str,scene["durations"]))+"};")
    lines.append("const passport_motion_scene_t passport_motion_scenes[] = {")
    lines.append('    {1,"动态角色 · 眨眼与回应",passport_motion_frames,NULL,18,true},')
    for scene in scenes:
        key=scene["key"]
        lines.append(f'    {{{scene["id"]},{json.dumps(scene["name"],ensure_ascii=False)},scene_{key}_frames,scene_{key}_durations,6,false}},')
    lines.extend(["};",f"const size_t passport_motion_scene_count = {1+len(scenes)};"])
    source = "\n".join(lines) + "\n"
    manifest = {"schema": 1, "private": not public, "format": "RGB565A8", "width": WIDTH,
                "height": HEIGHT, "frame_count": 18, "frame_bytes": WIDTH*HEIGHT*3,
                "unique_frames": len(unique), "pixel_bytes": sum(len(image[3]) for image in unique), "frames": records,
                "scene_count":1+len(scenes),"scenes":[{"id":1,"frame_count":18,"format":"RGB565A8"}]+
                [{"id":s["id"],"frame_count":6,"format":s["format"],"durations":s["durations"]} for s in scenes],
                "source_sha256": hashlib.sha256(source.encode()).hexdigest()}
    (output / "images").mkdir(parents=True, exist_ok=True)
    (output / "passport").mkdir(parents=True, exist_ok=True)
    (output / "images/passport_motion.c").write_text(source, encoding="utf-8", newline="\n")
    (output / "passport/motion-manifest.json").write_text(
        json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scene-pack",type=Path)
    parser.add_argument("--scene",action="append",choices=[key for key,_,_ in SCENES],
                        help="Scene to include from --scene-pack (repeatable; default all)")
    parser.add_argument("--public",action="store_true",help="Write the committed public showcase")
    args = parser.parse_args()
    manifest = generate_motion(args.pack, args.output,scene_pack=args.scene_pack,
                               scene_keys=tuple(args.scene or (key for key,_,_ in SCENES)),public=args.public)
    print(f"Motion assets: {manifest['frame_count']} frames, {manifest['pixel_bytes']} Flash pixel bytes")


if __name__ == "__main__":
    main()
