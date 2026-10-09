#!/usr/bin/env python3
"""Independent inverse-pixel check of R46 frames against original MD mappings."""
from pathlib import Path
import argparse, json, re, zipfile
from md_compression import nemesis_decompress
from source_ability_art_sources import SOURCE_ABILITY_ART

R=Path(__file__).resolve().parents[1]; O=R.parent/'build_artifacts'
raw=(R/'references/roms/ps4built.bin').read_bytes()
bank=(R/'gba/source_ability_effects.bin').read_bytes()
text=(R/'gba/source_ability_effects.c').read_text()
ap=argparse.ArgumentParser();ap.add_argument('--baseline-zip',type=Path);ap.add_argument('--allow-combat-bank-repairs',action='store_true');args=ap.parse_args();base=args.baseline_zip
fallback=R.parents[2]/'r46_recovery/PSIV_R45_FULL_CLAUDE_HANDOFF_2026-10-01.zip'
if base is None and fallback.exists():base=fallback
def rows(text,name):
    m=re.search(r'\b'+name+r'\[[^]]*\]=\{(.*?)\n\};',text,re.S); assert m
    return [[int(q.removesuffix('u'),0) for q in re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+)u?',line)] for line in m[1].splitlines() if '{' in line]
parts=rows(text,'ps4_source_ability_parts'); anims=rows(text,'ps4_source_ability_anims')
if base:
    with zipfile.ZipFile(base) as z:
        root='R46' if 'R46/source/gba/source_ability_effects.c' in z.namelist() else 'R45'
        old_text=z.read(root+'/source/gba/source_ability_effects.c').decode()
        old_bank=z.read(root+'/source/gba/source_ability_effects.bin')
        for name in ['source_enemy_effects.bin','source_combo_effects.bin']:
            if not args.allow_combat_bank_repairs:assert (R/'gba'/name).read_bytes()==z.read(root+'/source/gba/'+name),name
    old_parts=rows(old_text,'ps4_source_ability_parts'); old_anims=rows(old_text,'ps4_source_ability_anims')
    assert len(parts)==len(old_parts)==167 and len(anims)==len(old_anims)==132
    for p,q in zip(parts,old_parts): assert p[1:]==q[1:]
    for a,b in zip(anims,old_anims): assert a[:3]+a[4:]==b[:3]+b[4:]
    assert text[text.index('const Ps4SourceAbilityDesc'):]==old_text[old_text.index('const Ps4SourceAbilityDesc'):]
assert len(parts)==167 and len(anims)==132

def u16(p):return int.from_bytes(raw[p:p+2],'big')
def source_frames(table):
    count=u16(table); result=[]
    for i in range(count):
        rel=u16(table+2+i*2);rel=rel-65536 if rel&32768 else rel
        p=table+2+rel; hold=raw[p]; p+=1
        if raw[p]&128: result.append((hold,[])); continue
        sprites=[]
        for j in range(raw[p]+1):
            b=raw[p+1+j*6:p+7+j*6]
            x=b[4]-256 if b[4]&128 else b[4]
            y=b[0]-256 if b[0]&128 else b[0]
            sprites.append((x,y,(((b[1]>>2)&3)+1)*8,((b[1]&3)+1)*8,(b[2]<<8)|b[3]))
        result.append((hold,sprites))
    return result
def size(geom):
    return [ [(8,8),(16,16),(32,32),(64,64)], [(16,8),(32,8),(32,16),(64,32)], [(8,16),(8,32),(16,32),(32,64)] ][geom&3][(geom>>2)&3]
arts={p:nemesis_decompress(raw[p:]) for p in set(SOURCE_ABILITY_ART.values())}
checked=frames=pixels=unchanged=0
for ai,a in enumerate(anims):
    first,n,nf,hoff,table=a
    if base:
        b=old_anims[ai]; assert bank[hoff:hoff+nf]==old_bank[b[3]:b[3]+nf]
    if table not in SOURCE_ABILITY_ART:
        if base:
            for i in range(first,first+n):
                p,q=parts[i],old_parts[i]
                assert bank[p[0]:p[0]+p[1]*nf]==old_bank[q[0]:q[0]+q[1]*nf]
        unchanged+=1; continue
    art=arts[SOURCE_ABILITY_ART[table]]; source=source_frames(table)
    assert len(source)==nf and bytes(h for h,_ in source)==bank[hoff:hoff+nf]
    for i in range(first,first+n):
        off,fb,geom,_,ox,oy,_=parts[i]; w,h=size(geom); assert fb==w*h//2
        for f,(_,sprites) in enumerate(source):
            packed=bank[off+f*fb:off+(f+1)*fb]; assert len(packed)==fb
            for dy in range(h):
                for dx in range(w):
                    # Invert the source mapping at this destination pixel.
                    # Do not reuse the repair renderer or its packer.
                    expected=0; wx,wy=ox+dx,oy+dy
                    for sx,sy,sw,sh,prop in sprites:
                        if not (sx<=wx<sx+sw and sy<=wy<sy+sh):continue
                        ux,uy=wx-sx,wy-sy
                        if prop&0x800:ux=sw-1-ux
                        if prop&0x1000:uy=sh-1-uy
                        tile=(prop&0x7ff)+(ux//8)*(sh//8)+uy//8
                        addr=tile*32+(uy%8)*4+(ux%8)//2; assert addr<len(art)
                        ci=(art[addr]>>(0 if ux&1 else 4))&15
                        if ci:expected=ci
                    at=((dy//8)*(w//8)+dx//8)*32+(dy%8)*4+(dx%8)//2
                    actual=(packed[at]>>(4 if dx&1 else 0))&15
                    assert actual==expected,(hex(table),i,f,dx,dy,actual,expected)
                    pixels+=1
            frames+=1
        checked+=1
report={'result':'PASS','parts_checked':checked,'part_frames_checked':frames,'pixels_checked':pixels,
        'baseline_archive':str(base) if base else None,
        'retained_raw_animation_records':unchanged if base else None,'all_132_animation_holds_unchanged':True if base else None,
        'all_167_part_geometry_unchanged':True if base else None,'ability_descriptors_unchanged':True if base else None,
        'enemy_and_combo_banks_unchanged':True if base and not args.allow_combat_bank_repairs else None,
        'enemy_and_combo_scope':'Validated separately by check_r46_combat_pixels.py' if args.allow_combat_bank_repairs else 'Preserved baseline',
        'scope':'Independent inverse-pixel comparison to original Nemesis/mapping bytes; does not certify every controller or hardware display.'}
(O/'R46_ABILITY_PIXEL_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS original ability pixels:',checked,'parts,',frames,'part frames,',pixels,'pixels')
