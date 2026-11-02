#!/usr/bin/env python3
"""Build deterministic, licensed fonts and three RGB565 avatar frames."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from configure_passport import normalize_profile,unique_pairs

ROOT=Path(__file__).resolve().parents[1]
FONT_SHA256="a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da"
FONT_REVISION="2894aab31764f10f29c421bdfd2340d3b382d384"
FONT_URL=f"https://raw.githubusercontent.com/google/fonts/{FONT_REVISION}/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf"

def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def collect_glyphs(ui: dict,profile: dict | None=None) -> list[int]:
    result=set(range(32,127))
    # GB2312's 6,763 Han characters let users edit ordinary Chinese names on
    # the device without regenerating a personal subset for every change.
    for high in range(0xb0,0xf8):
        for low in range(0xa1,0xff):
            try: result.add(ord(bytes((high,low)).decode("gb2312")))
            except UnicodeDecodeError: pass
    result.update(map(ord,"，。！？：；、（）《》【】“”‘’—…·￥％　"))
    for text in [*ui.values(),*(profile or {}).values()]:
        if not isinstance(text,str):
            raise ValueError("Asset text values must be strings")
        result.update(ord(c) for c in text if c not in "\n\r\t")
    return sorted(result)

def validate_destination(output: Path,private: bool) -> None:
    output=output.resolve()
    if private and output.is_relative_to(ROOT):
        result=subprocess.run(["git","check-ignore","--quiet","--",str(output)],
                              cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        if result.returncode:
            raise ValueError("Personal assets require a Git-ignored output directory")

def _letterbox(source, box_w: int, box_h: int):
    """Contain-fit an RGBA image into a fixed transparent frame, centered."""
    from PIL import Image
    scale=min(box_w/source.width,box_h/source.height)
    width=max(1,min(box_w,round(source.width*scale)))
    height=max(1,min(box_h,round(source.height*scale)))
    resized=source.resize((width,height),Image.Resampling.LANCZOS)
    placed=Image.new("RGBA",(box_w,box_h),(0,0,0,0))
    placed.paste(resized,((box_w-width)//2,(box_h-height)//2),resized)
    return placed

def _avatar_frames(avatar: Path | None):
    """Return (frames,width,height,has_alpha). Real avatars letterbox into a fixed
    76x150 transparent frame so any scene aspect keeps clear of the info column."""
    from PIL import Image,ImageDraw,ImageEnhance
    if avatar:
        with Image.open(avatar) as image:
            source=image.convert("RGBA")
        # Two layout frames: near-square/wide scenes get a wide canvas (stacked
        # badge layout with compact info below); tall scenes get a narrow column.
        box_w,box_h=(216,112) if source.width/source.height>=0.85 else (76,150)
        placed=_letterbox(source,box_w,box_h)
        frames=[]
        for level in (1.0,0.96,0.92):
            dimmed=ImageEnhance.Brightness(placed.convert("RGB")).enhance(level)
            dimmed.putalpha(placed.getchannel("A"))
            frames.append(dimmed)
        return frames,box_w,box_h,True
    frames=[]
    for state in range(3):
        image=Image.new("RGB",(64,64),(11,19,32))
        draw=ImageDraw.Draw(image)
        draw.ellipse((3,3,60,60),fill=(34,68,77))
        draw.ellipse((13,10,50,49),fill=(207,232,225))
        draw.ellipse((13,45,50,76),fill=(53,150,149))
        for x in (23,38):
            if state==0: draw.ellipse((x,25,x+3,29),fill=(15,32,43))
            elif state==1: draw.rectangle((x-1,27,x+3,28),fill=(15,32,43))
            else: draw.line((x-1,28,x+3,28),fill=(15,32,43),width=1)
        draw.arc((25,28,39,41),start=10,end=170,fill=(48,89,90),width=2)
        frames.append(image)
    return frames,64,64,False

def _image_source(frames,width: int,height: int,alpha: bool,temporary: bool) -> str:
    result=['/* Local avatar frames; the default uses neutral geometric artwork. */',
            '#include "passport_assets.h"']
    pixel_bytes=width*height*2
    total=pixel_bytes+(width*height if alpha else 0)
    for index,image in enumerate(frames):
        pixels=bytearray()
        for r,g,b in image.convert("RGB").getdata():
            value=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
            pixels.extend((value&255,value>>8))
        if alpha:
            pixels.extend(image.getchannel("A").tobytes())
        if len(pixels)!=total:
            raise ValueError("Invalid avatar pixel size")
        result.append(f"static const uint8_t passport_avatar_pixels_{index}[{total}] = {{")
        result += ["    "+",".join(f"0x{x:02x}" for x in pixels[at:at+24])+"," for at in range(0,len(pixels),24)]
        result.append("};")
    result.append("const lv_image_dsc_t passport_avatar_frames[3] = {")
    cf="LV_COLOR_FORMAT_RGB565A8" if alpha else "LV_COLOR_FORMAT_RGB565"
    for index in range(3):
        result.append(f"    {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf={cf},.w={width},.h={height},.stride={width*2}}},"
                      f".data_size={total},.data=passport_avatar_pixels_{index}"+"},")
    result.append("};")
    result.append("const bool passport_avatar_is_temporary="+("true" if temporary else "false")+";")
    return "\n".join(result)+"\n"

def _gallery_entries(gallery: Path | None) -> list[tuple[str,Path | None]]:
    """[(title, png path)] scene list embedded as the selectable gallery.
    Without a pack the public build embeds the procedural default scene."""
    if gallery is None:
        return [("默认",None)]
    manifest=json.loads((gallery/"manifest.json").read_text(encoding="utf-8"))
    entries=[(scene["title_zh"],gallery/scene["png"]) for scene in manifest["scenes"]]
    if not entries:
        raise ValueError("Gallery pack has no scenes")
    return entries

def _gallery_source(entries: list[tuple[str,Path | None]]) -> str:
    from PIL import Image
    lines=['/* Embedded pixel-scene gallery; each scene letterboxes into a badge frame. */',
           '#include "passport_assets.h"']
    dscs=[]
    for index,(name,path) in enumerate(entries):
        if path is None:
            frames,_,_,_=_avatar_frames(None)
            placed=frames[0].convert("RGBA")
        else:
            with Image.open(path) as image:
                source=image.convert("RGBA")
            box_w,box_h=(216,112) if source.width/source.height>=0.85 else (76,150)
            placed=_letterbox(source,box_w,box_h)
        width,height=placed.size
        pixels=bytearray()
        for r,g,b in placed.convert("RGB").getdata():
            value=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
            pixels.extend((value&255,value>>8))
        pixels.extend(placed.getchannel("A").tobytes())
        total=len(pixels)
        lines.append(f"static const uint8_t passport_gallery_pixels_{index}[{total}] = {{")
        lines += ["    "+",".join(f"0x{x:02x}" for x in pixels[at:at+24])+"," for at in range(0,len(pixels),24)]
        lines.append("};")
        dscs.append((index,width,height,total))
    lines.append("const lv_image_dsc_t passport_gallery_frames[] = {")
    for index,width,height,total in dscs:
        lines.append(f"    {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8,"
                     f".w={width},.h={height},.stride={width*2}}},.data_size={total},"
                     f".data=passport_gallery_pixels_{index}}},")
    lines.append("};")
    lines.append("const char *const passport_gallery_names[] = {"+
                 ",".join(json.dumps(name,ensure_ascii=False) for name,_ in entries)+"};")
    lines.append("const size_t passport_gallery_count="
                 "sizeof passport_gallery_frames/sizeof passport_gallery_frames[0];")
    return "\n".join(lines)+"\n"

def _glyph_source(glyphs: list[int]) -> str:
    values=",".join(f"0x{cp:x}" for cp in glyphs)
    return (
        '#include "passport_assets.h"\n'
        f"const uint32_t passport_asset_glyphs[] = {{{values}}};\n"
        "const size_t passport_asset_glyph_count=sizeof passport_asset_glyphs/sizeof passport_asset_glyphs[0];\n"
        "bool passport_assets_has_glyph(uint32_t cp,void *user) {\n"
        "    (void)user; size_t lo=0,hi=passport_asset_glyph_count;\n"
        "    while(lo<hi) { size_t mid=lo+(hi-lo)/2;\n"
        "        if(passport_asset_glyphs[mid]<cp) lo=mid+1; else hi=mid; }\n"
        "    return lo<passport_asset_glyph_count && passport_asset_glyphs[lo]==cp;\n"
        "}\n"
    )

def generate_assets(*,font: Path,converter: Path,node: Path,output: Path,
                    profile: dict | None=None,avatar: Path | None=None,
                    gallery: Path | None=None,license_path: Path | None=None,
                    motion_pack: Path | None=None,scene_pack: Path | None=None) -> dict:
    from fontTools.ttLib import TTFont
    from fontTools import subset
    from fontTools.varLib.instancer import instantiateVariableFont

    private=profile is not None or avatar is not None or gallery is not None or motion_pack is not None or scene_pack is not None
    if private:
        local_motion=ROOT/"assets/private/passport-motion"
        local_scenes=ROOT/"assets/private/passport-scenes"
        if motion_pack is None and (local_motion/"animation-manifest.json").is_file():motion_pack=local_motion
        if scene_pack is None and (local_scenes/"animation-manifest.json").is_file():scene_pack=local_scenes
    if scene_pack is not None and motion_pack is None:
        raise ValueError("Scene animation requires the default motion pack")
    validate_destination(output,private)
    profile=normalize_profile(profile) if profile is not None else None
    if digest(font)!=FONT_SHA256:
        raise ValueError("Unexpected font source checksum")
    ui=json.loads((ROOT/"assets"/"passport"/"ui-text.json").read_text(encoding="utf-8"))
    glyphs=collect_glyphs(ui,profile)
    with TTFont(font,recalcTimestamp=False) as source:
        missing=set(glyphs)-source.getBestCmap().keys()
        if missing:
            raise ValueError(f"Missing source glyphs: {len(missing)}")
        options=subset.Options()
        cutter=subset.Subsetter(options=options)
        cutter.populate(unicodes=glyphs)
        cutter.subset(source)
        if "fvar" in source:
            instantiateVariableFont(source,{"wght":400},inplace=True)
        source.recalcTimestamp=False
        version=subprocess.run([str(node),str(converter),"--version"],check=True,capture_output=True,text=True)
        if version.stdout.strip()!="1.5.3":
            raise ValueError("Expected lv_font_conv 1.5.3")
        for part in ("fonts","images","passport"):
            (output/part).mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="passport-font-") as temporary:
            work=Path(temporary)
            source.save(work/"font.ttf")
            # A codepoint list exceeds Windows' 32K command-line limit. Unicode
            # symbols keep this argument below 7K UTF-16 characters.
            symbols="".join(map(chr,glyphs))
            for size in (16,20):
                name=f"passport_font_{size}"
                converted=subprocess.run([str(node),str(converter),"--font","font.ttf","--symbols",symbols,
                                          "--size",str(size),"--bpp","4","--format","lvgl","--no-compress",
                                          "--lv-font-name",name,"--lv-include","lvgl.h","--output",name+".c"],
                                         cwd=work,capture_output=True,text=True)
                if converted.returncode:
                    raise ValueError(f"Font converter failed for size {size}, exit {converted.returncode}")
                prelude="/* Derived from Noto Sans SC; SIL OFL 1.1. See NotoSansSC-OFL.txt. */\n"
                (output/"fonts"/(name+".c")).write_text(prelude+(work/(name+".c")).read_text(encoding="utf-8"),encoding="utf-8",newline="\n")
    license_path=license_path or font.parent/"OFL.txt"
    license_text=license_path.read_text(encoding="utf-8")
    if "SIL OPEN FONT LICENSE" not in license_text:
        raise ValueError("Missing font license notice")
    (output/"fonts"/"NotoSansSC-OFL.txt").write_text(license_text,encoding="utf-8",newline="\n")
    frames,avatar_width,avatar_height,avatar_alpha=_avatar_frames(avatar)
    (output/"images"/"passport_avatar.c").write_text(_image_source(frames,avatar_width,avatar_height,avatar_alpha,avatar is None),encoding="utf-8",newline="\n")
    gallery_entries=_gallery_entries(gallery)
    (output/"images"/"passport_gallery.c").write_text(_gallery_source(gallery_entries),encoding="utf-8",newline="\n")
    motion_manifest=None
    if motion_pack is not None:
        from build_passport_motion import generate_motion
        motion_manifest=generate_motion(motion_pack,output,scene_pack=scene_pack)
    else:
        (output/"images/passport_motion.c").write_text(
            (ROOT/"assets/images/passport_motion.c").read_text(encoding="utf-8"),encoding="utf-8",newline="\n")
    (output/"passport"/"passport_glyphs.c").write_text(_glyph_source(glyphs),encoding="utf-8",newline="\n")
    strings=["/* Generated from ui-text.json; font inventory uses the same copy. */","#pragma once"]
    for key,value in ui.items():
        if not key.isascii() or not key.isidentifier():
            raise ValueError("Invalid UI text key")
        strings.append("#define PASSPORT_TEXT_"+key.upper()+" "+json.dumps(value,ensure_ascii=False))
    (output/"passport"/"passport_strings.h").write_text("\n".join(strings)+"\n",encoding="utf-8",newline="\n")
    names=["fonts/passport_font_16.c","fonts/passport_font_20.c","fonts/NotoSansSC-OFL.txt",
           "images/passport_avatar.c","images/passport_gallery.c","images/passport_motion.c",
           "passport/passport_glyphs.c","passport/passport_strings.h"]
    manifest={"schema":1,"private":private,"font_source_sha256":FONT_SHA256,"font_source_revision":FONT_REVISION,
              "font_source_url":FONT_URL,"converter":"lv_font_conv 1.5.3","converter_sha256":digest(converter),
              "sizes":[16,20],"glyphs":glyphs,"avatar_temporary":avatar is None,
              "ui_sha256":digest(ROOT/"assets"/"passport"/"ui-text.json"),
              "avatar":{"frames":3,"width":avatar_width,"height":avatar_height,
                        "pixel_bytes":len(frames)*avatar_width*avatar_height*(3 if avatar_alpha else 2),"alpha":avatar_alpha},
              "gallery_scenes":len(gallery_entries),
              "motion_scenes":motion_manifest["scene_count"] if motion_manifest else 0,
              "files":{name:digest(output/name) for name in names}}
    (output/"passport"/"manifest.json").write_text(json.dumps(manifest,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
    return manifest

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font",type=Path,required=True)
    parser.add_argument("--font-conv",type=Path,required=True)
    parser.add_argument("--node",type=Path,default=os.environ.get("PASSPORT_NODE") or shutil.which("node"))
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--profile",type=Path)
    parser.add_argument("--avatar",type=Path)
    parser.add_argument("--gallery",type=Path,help="Avatar pack directory (manifest.json + png/)")
    parser.add_argument("--motion-pack",type=Path,help="Default motion source; auto-detected for personal builds")
    parser.add_argument("--scene-pack",type=Path,help="Optional night/cat motion source")
    args=parser.parse_args()
    if not args.node: parser.error("Provide --node or PASSPORT_NODE")
    profile=None
    if args.profile:
        with args.profile.open(encoding="utf-8-sig") as stream:
            profile=json.load(stream,object_pairs_hook=unique_pairs)
    manifest=generate_assets(font=args.font,converter=args.font_conv,node=Path(args.node),
                             output=args.output,profile=profile,avatar=args.avatar,gallery=args.gallery,
                             motion_pack=args.motion_pack,scene_pack=args.scene_pack)
    print(f"Generated {len(manifest['glyphs'])} glyphs, {manifest['gallery_scenes']} gallery scenes"
          f" and 3 avatar frames; private={manifest['private']}")
    return 0

if __name__=="__main__": raise SystemExit(main())
