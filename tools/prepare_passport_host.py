#!/usr/bin/env python3
"""Fetch hash-pinned host-only sources, without installing the ESP-IDF toolchain."""
from __future__ import annotations
import argparse
import hashlib
import io
from pathlib import Path,PurePosixPath
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
LVGL_SHA="34a955cdf3a2d005507b704e87357af669a114523b6d3f77b5344fdc68717bc6"
CJSON_REV="c859b25da02955fef659d658b8f324b5cde87be3"
CJSON_FILES={
 "cJSON.c":"298581a04a36c0165da4b0aade235c23088cb2faa58651d720ea2f3706ed0b0d",
 "cJSON.h":"25b0145150d500498e4d209cec69c18c42cf818bffcc54690be3b895a2a16dee",
 "LICENSE":"a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c"}
def fetch(url: str,path: Path,digest: str,maximum: int) -> bytes:
    if path.is_file():
        data=path.read_bytes()
        if hashlib.sha256(data).hexdigest()==digest: return data
        raise ValueError("Cached source hash mismatch: "+path.name)
    with urllib.request.urlopen(url,timeout=30) as response:
        data=response.read(maximum+1)
    if len(data)>maximum or hashlib.sha256(data).hexdigest()!=digest:
        raise ValueError("Downloaded source size/hash mismatch: "+path.name)
    path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);return data
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache",type=Path,default=ROOT/"build"/"host-deps")
    args=parser.parse_args();cache=args.cache.resolve();cache.mkdir(parents=True,exist_ok=True)
    for name,digest in CJSON_FILES.items():
        fetch(f"https://raw.githubusercontent.com/DaveGamble/cJSON/{CJSON_REV}/{name}",
              cache/"cjson"/name,digest,1024*1024)
    marker=cache/"lvgl"/".passport-source"
    marker_value=LVGL_SHA+":headers-v2"
    if not marker.is_file() or marker.read_text()!=marker_value:
        data=fetch("https://codeload.github.com/lvgl/lvgl/tar.gz/refs/tags/v9.5.0",
                   cache/"lvgl-9.5.0.tar.gz",LVGL_SHA,110*1024*1024)
        total=0
        with tarfile.open(fileobj=io.BytesIO(data),mode="r:gz") as archive:
            for member in archive:
                relative=PurePosixPath(*PurePosixPath(member.name).parts[1:])
                if not relative.parts or not (relative.parts[0]=="src" or
                    (len(relative.parts)==1 and relative.suffix==".h") or str(relative)=="LICENCE.txt"):
                    continue
                if member.isdir(): continue
                if not member.isfile() or relative.is_absolute() or ".." in relative.parts:
                    raise ValueError("Unsafe archive member")
                total+=member.size
                if total>40*1024*1024: raise ValueError("Expanded source exceeds limit")
                destination=cache/"lvgl"/Path(*relative.parts)
                destination.parent.mkdir(parents=True,exist_ok=True)
                source=archive.extractfile(member)
                if source is None: raise ValueError("Missing archive member")
                destination.write_bytes(source.read())
        marker.write_text(marker_value)
    print("Hash-pinned cJSON and LVGL host sources: ready")
if __name__=="__main__":
    main()
