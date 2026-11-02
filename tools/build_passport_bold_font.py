#!/usr/bin/env python3
"""Build the licensed 24 px weight-700 badge font without changing other assets."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from build_passport_assets import ROOT,FONT_SHA256,collect_glyphs,validate_destination

def generate(font,converter,node,output,glyph_manifest=None):
    if hashlib.sha256(font.read_bytes()).hexdigest()!=FONT_SHA256:raise ValueError('Unexpected pinned font source')
    glyphs=collect_glyphs(json.loads((ROOT/'assets/passport/ui-text.json').read_text(encoding='utf-8')))
    private=False
    if glyph_manifest:
        inventory=json.loads(glyph_manifest.read_text(encoding='utf-8'));private=bool(inventory.get('private'))
        glyphs=sorted(set(glyphs)|set(inventory['glyphs']))
    validate_destination(output,private)
    license_text=(font.parent/'OFL.txt').read_text(encoding='utf-8')
    if 'SIL OPEN FONT LICENSE' not in license_text:raise ValueError('Missing font license notice')
    version=subprocess.run([str(node),str(converter),'--version'],capture_output=True,text=True,check=True)
    if version.stdout.strip()!='1.5.3':raise ValueError('Expected lv_font_conv 1.5.3')
    with TTFont(font,recalcTimestamp=False) as source:
        if set(glyphs)-source.getBestCmap().keys():raise ValueError('Missing source glyphs')
        cutter=subset.Subsetter(options=subset.Options());cutter.populate(unicodes=glyphs);cutter.subset(source)
        instantiateVariableFont(source,{'wght':700},inplace=True);source.recalcTimestamp=False
        with tempfile.TemporaryDirectory(prefix='passport-bold-') as folder:
            work=Path(folder);source.save(work/'font.ttf')
            subprocess.run([str(node),str(converter),'--font','font.ttf','--symbols',''.join(map(chr,glyphs)),
                '--size','24','--bpp','2','--format','lvgl','--no-compress',
                '--lv-font-name','passport_font_24_bold','--lv-include','lvgl.h','--output','bold.c'],
                cwd=work,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            content='/* Noto Sans SC, 24 px, weight 700; SIL OFL 1.1. See NotoSansSC-OFL.txt. */\n'+(work/'bold.c').read_text(encoding='utf-8')
    (output/'fonts').mkdir(parents=True,exist_ok=True);(output/'passport').mkdir(parents=True,exist_ok=True)
    license_path=output/'fonts/NotoSansSC-OFL.txt'
    if not license_path.exists():license_path.write_text(license_text,encoding='utf-8',newline='\n')
    destination=output/'fonts/passport_font_24_bold.c';destination.write_text(content,encoding='utf-8',newline='\n')
    manifest={'schema':1,'private':private,'size':24,'weight':700,'bpp':2,'glyphs':glyphs,
        'font_source_sha256':FONT_SHA256,'source_sha256':hashlib.sha256(destination.read_bytes()).hexdigest()}
    (output/'passport/bold-font-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(f'24 px bold: {len(glyphs)} glyphs, source {len(content)} bytes')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('font','font-conv','node','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--glyph-manifest',type=Path);a=p.parse_args()
    generate(a.font,a.font_conv,a.node,a.output,a.glyph_manifest)
