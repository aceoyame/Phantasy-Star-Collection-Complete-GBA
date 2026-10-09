#!/usr/bin/env python3
"""Generate source-backed PSIV TECH/SKILL BattleObj animation frames.

This is deliberately not a fallback-art generator. Every emitted frame is rendered
from the verified PSIV ROM's original BattleObj mapping table + ability PLC art.
Special non-sprite controllers are flagged for explicit runtime handling.
"""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from md_compression import nemesis_decompress
from generate_enemy_animation_frames import Rom
from generate_technique_objects import plc_chunks, anim_table
from generate_enemy_attack_frames import records, bounds, split, legal
from source_ability_art_sources import SOURCE_ABILITY_ART

ROOT=Path(__file__).resolve().parent.parent
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
BASEPAL=0x296300; SK_PALSEL=0x9CCA; SK_PALWORDS=0x9D32

TECH_ANIMS={
1:[0x25b008,0x25b242,0x25af70],2:[0x25b008,0x25b242,0x25af70],3:[0x25b3d8,0x25b2b4],4:[0x25b008,0x25b720,0x25af70],5:[0x25b008,0x25b720,0x25af70],6:[0x25b3d8,0x25b7ae],7:[0x25b8a0],8:[0x25b968,0x25b8ac],9:[0x25b982,0x25b8c8],10:[0x25bfbe],11:[0x25c240],12:[0x25c762,0x25cc6e],13:[0x25ccc0],14:[0x25ccc0],15:[0x25ccc0],16:[0x25cd36],17:[],18:[0x25cde0],19:[0x25cde0],20:[0x25cfc0],21:[0x25cfc0],22:[0x25cffe],23:[0x25d0c4],24:[0x25d542],25:[0x25d542],26:[0x25d542],27:[0x25d542],28:[0x25d542],29:[0x25d542],30:[0x25d1de],31:[0x25d1de],32:[0x25d240],33:[0x25d1de],34:[0x25d6c0],35:[0x25d6c0],36:[0x25d708],37:[0x25d708],38:[0x25d6c0]}
SKILL_ANIMS={
1:[0x25e68a],2:[0x25e5fe],3:[],4:[0x25d9b6,0x25b242,0x25af70],5:[0x25da98,0x25b242,0x25af70],6:[0x25fdde,0x25fdf0,0x25e58c,0x25e5a8],7:[0x25db0a,0x25bf14],8:[0x25e804,0x25e824],9:[],10:[0x25dbac],11:[0x25dd1c],12:[0x25de64,0x25b2b4],13:[0x25bf14],14:[0x25be8e],15:[0x25e050,0x25dfb4,0x25b0c8,0x25af70],16:[0x25e804,0x25e824],17:[0x25e072,0x25bace],18:[0x25be8e],19:[0x25be8e],20:[0x25db0a,0x25bf14],21:[0x25e0a6],22:[0x25b008],23:[0x25e160,0x25ccc0],24:[0x2608ee,0x25eb44],25:[0x25e226],26:[0x25e226],27:[0x25b0c8,0x25b008,0x25e2de,0x25ccc0],28:[],29:[0x25af70,0x25cfc0],30:[0x25cfc0],31:[0x25f7ac,0x25e54e,0x25d0c4],32:[0x25ccc0,0x25bace],33:[0x25d6c0,0x25e35e],34:[0x25eae2],35:[0x25da98,0x25af70],36:[0x25af70,0x25d0c4],37:[0x25d0c4],38:[0x25e3fa],39:[0x25d39c,0x25b008,0x25b0c8,0x25d916,0x25b62e],40:[0x25ce88,0x25d240],41:[0x25af70,0x25d240],42:[0x25e4f8],43:[0x25d39c,0x25d542],44:[0x25d542],45:[0x25d708],46:[0x25d6c0],47:[0x25d6c0],48:[0x25e0a6],49:[0x25d9b6,0x25b242,0x25af70],50:[0x25dbac],51:[0x25dd1c],52:[0x25e160,0x25ccc0],53:[0x25e3fa],54:[0x25e4f8]}
# Kind flags: 1 normal generated sprite sequence, 2 source palette/screen controller,
# 3 source weapon animation path, 4 custom raw source sprite package.
TECH_SPECIAL={17:2}
SKILL_SPECIAL={3:3,6:5,9:3,28:4,31:5,47:5}
# Exact source SFX IDs. Zero means the source root is intentionally silent here;
# multi-cue skills keep their richer source schedule in title_runtime_v06.c.
TECH_SFX={1:0xBE,2:0xBE,3:0xC3,4:0xC4,5:0xC4,6:0xC4,7:0xC5,8:0xC5,9:0xC5,10:0xC6,11:0xC6,12:0xC6,13:0xC7,14:0xC7,15:0xC7,16:0xC0,17:0xCB,18:0xC8,19:0xC8,20:0xCA,21:0xCA,22:0xCB,23:0xCA,24:0xCC,25:0xCC,26:0xCC,27:0xCC,28:0xCC,29:0xCC,30:0xC9,31:0xC9,32:0xDC,33:0xC9,34:0xC4,35:0xC4,36:0xCD,37:0xCD,38:0xC4}

OBJ_GEOM={
    (8,8):(0,0),(16,16):(0,1),(32,32):(0,2),(64,64):(0,3),
    (16,8):(1,0),(32,8):(1,1),(32,16):(1,2),(64,32):(1,3),
    (8,16):(2,0),(8,32):(2,1),(16,32):(2,2),(32,64):(2,3),
}
def obj_part_meta(w,h,cx,cy):
    shape,size=OBJ_GEOM[(w,h)]
    ox,oy=cx-w//2,cy-h//2
    if not (-128<=ox<=127 and -128<=oy<=127):raise ValueError(f'OBJ part offset out of int8 range: {(w,h,cx,cy,ox,oy)}')
    return shape|(size<<2),ox,oy

def md2gba(c:int)->int:
    cv=(0,4,9,13,18,22,27,31);r=(c>>1)&7;g=(c>>5)&7;b=(c>>9)&7
    return cv[r]|(cv[g]<<5)|(cv[b]<<10)

def palette_words(rom:Rom,kind:str,aid:int):
    w=[int.from_bytes(rom.read(BASEPAL+i*2,2),'big') for i in range(16)]
    if kind=='T':
        sel=rom.read(0x9AB0+aid-1,1)[0]; p=0x9AD6+sel*8
        w[10:14]=[int.from_bytes(rom.read(p+i,2),'big') for i in range(0,8,2)]
    else:
        sel=rom.read(SK_PALSEL+aid,1)[0]
        if sel:
            p=SK_PALWORDS+sel*8;w[10:14]=[int.from_bytes(rom.read(p+i,2),'big') for i in range(0,8,2)]
    return [md2gba(x) for x in w]

def art_pool(rom:Rom,kind:str,aid:int):
    idx=aid+(0x4F if kind=='T' else 0x8C);start=0x9B80
    if kind=='S' and idx in (0x92,0xA4): start=0x9C40
    pool=bytearray(0x2000); off=start-0x9B80
    chunks,_,_=plc_chunks(rom,idx)
    for ch in chunks:
        if off+len(ch)>len(pool): raise ValueError((kind,aid,'primary PLC overflow'))
        pool[off:off+len(ch)]=ch;off+=len(ch)
    # Death's projectile retains the high primary PLC and then overlays PLC $C4 at $9B80.
    if kind=='S' and aid==0x18:
        off=0; chs,_,_=plc_chunks(rom,0xC4)
        for ch in chs:
            if off+len(ch)>len(pool): raise ValueError('death overlay overflow')
            pool[off:off+len(ch)]=ch;off+=len(ch)
    return bytes(pool)

def render_indices(art:bytes,m:bytes,reg):
    x0,y0,x1,y1=reg;w=x1-x0;h=y1-y0;pix=bytearray(w*h);tc=len(art)//32
    for x,y,tw,th,p in records(m):
        bt=p&0x7ff;hf=bool(p&0x0800);vf=bool(p&0x1000)
        for tx in range(tw):
            for ty in range(th):
                stx=tw-1-tx if hf else tx;sty=th-1-ty if vf else ty;ti=bt+stx*th+sty
                if ti>=tc: raise ValueError(f'tile {ti} >= {tc}')
                tile=art[ti*32:(ti+1)*32]
                for py in range(8):
                    sy=7-py if vf else py;wy=y+ty*8+py
                    if wy<y0 or wy>=y1: continue
                    for px in range(8):
                        sx=7-px if hf else px;wx=x+tx*8+px
                        if wx<x0 or wx>=x1: continue
                        q=tile[sy*4+(sx>>1)];ci=(q>>4) if not(sx&1) else q&15
                        if ci: pix[(wy-y0)*w+(wx-x0)]=ci
    return pix,w,h

def pack_canvas(pix,sw,sh,cw,ch):
    px=(cw-sw)//2;py=(ch-sh)//2;out=bytearray(cw*ch//2)
    # GBA OBJ 4bpp 1D tile order; pixel 0 = low nibble.
    o=0
    for ty in range(0,ch,8):
      for tx in range(0,cw,8):
       for y in range(8):
        for x in range(0,8,2):
          def get(xx):
            sx=tx+xx-px;sy=ty+y-py
            return pix[sy*sw+sx] if 0<=sx<sw and 0<=sy<sh else 0
          out[o]=get(x)|(get(x+1)<<4);o+=1
    return bytes(out)

def raw_illusion(rom:Rom):
    # loc_25FCCA: long raw art pointer, then one ordinary sprite mapping.
    p=0x25FCCA; artptr=int.from_bytes(rom.read(p,4),'big')&0xffffff; art=rom.read(artptr,0x3A0)
    n=rom.read(p+4,1)[0]+1;m=rom.read(p+4,1+n*6)
    return art,m

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args()
    raw=a.rom.read_bytes();sha=hashlib.sha256(raw).hexdigest()
    if len(raw)!=0x300000 or sha!=EXPECTED:raise SystemExit(f'wrong PSIV ROM {len(raw)} {sha}')
    rom=Rom(raw);bank=bytearray();parts=[];anims=[];abilities={'T':[None]*39,'S':[None]*55};manifest={'source_rom_sha256':sha,'tech':{},'skill':{}}
    blob_cache={}
    def put(b:bytes,align=4):
        # Exact byte-level dedupe only: source frames/holds with identical
        # rendered payloads share ROM storage but retain separate descriptors.
        key=(align,bytes(b))
        if key in blob_cache:return blob_cache[key]
        while len(bank)%align:bank.append(0)
        o=len(bank);bank.extend(b);blob_cache[key]=o;return o
    for kind,table,maxid in [('T',TECH_ANIMS,38),('S',SKILL_ANIMS,54)]:
      for aid in range(1,maxid+1):
        special=(TECH_SPECIAL if kind=='T' else SKILL_SPECIAL).get(aid,0);first_anim=len(anims);used=[]
        pal=palette_words(rom,kind,aid)
        if special==4:
            art,m=raw_illusion(rom); bs=bounds(m);regs=split(bs);first_part=len(parts)
            for r in regs:
                pix,sw,sh=render_indices(art,m,r);cw,ch=legal(sw,sh);dat=pack_canvas(pix,sw,sh,cw,ch);off=put(dat)
                cx,cy=(r[0]+r[2])//2,(r[1]+r[3])//2;geom,ox,oy=obj_part_meta(cw,ch,cx,cy);parts.append((off,len(dat),geom,0,ox,oy))
            ho=put(bytes([0x60]));anims.append((first_part,len(regs),1,ho,0x25FCCA));used=[0x25FCCA]
        elif not special:
            art=art_pool(rom,kind,aid)
            for tab in table.get(aid,[]):
                # Child mappings restart at tile zero in their own PLC bank.
                # The concatenated caster pool is valid only for the explicit
                # raw/composite controllers, such as Death's retained overlay.
                frame_art=nemesis_decompress(rom.tail(SOURCE_ABILITY_ART[tab])) if tab in SOURCE_ABILITY_ART else art
                try: maps,holds=anim_table(rom,tab)
                except Exception: continue
                vis=[m for m in maps if m is not None]
                if not vis: continue
                bs=[bounds(m) for m in vis];u=(min(x[0] for x in bs),min(x[1] for x in bs),max(x[2] for x in bs),max(x[3] for x in bs));regs=split(u)
                # Hard guarantee the live frame fits the 8 KiB upper Mode-4 OBJ scratch.
                live=0
                for r in regs:
                    cw,ch=legal(r[2]-r[0],r[3]-r[1]);live+=cw*ch//2
                if live>0x2000: continue
                first_part=len(parts)
                # Each part stores frames contiguously.
                for r in regs:
                    cw,ch=legal(r[2]-r[0],r[3]-r[1]);frames=[]
                    for m in maps:
                        if m is None: frames.append(bytes(cw*ch//2))
                        else:
                            try:
                                pix,sw,sh=render_indices(frame_art,m,r)
                            except Exception as e:
                                raise RuntimeError(f'{kind} {aid:02X} tab {tab:06X} reg {r}: {e}') from e
                            frames.append(pack_canvas(pix,sw,sh,cw,ch))
                    dat=b''.join(frames);off=put(dat);cx,cy=(r[0]+r[2])//2,(r[1]+r[3])//2;geom,ox,oy=obj_part_meta(cw,ch,cx,cy);parts.append((off,cw*ch//2,geom,0,ox,oy))
                ho=put(bytes(holds));anims.append((first_part,len(regs),len(maps),ho,tab));used.append(tab)
        flags=special if special else (1 if len(anims)>first_anim else 0)
        sfx=TECH_SFX.get(aid,0) if kind=='T' else 0
        abilities[kind][aid]=(first_anim,len(anims)-first_anim,flags,sfx,pal)
        manifest['tech' if kind=='T' else 'skill'][f'{aid:02X}']={'kind':flags,'source_tables':[f'{x:06X}' for x in used],'sfx':sfx,'palette':pal}
    # C descriptors/header.
    h=ROOT/'gba/source_ability_effects.h';c=ROOT/'gba/source_ability_effects.c';bpath=ROOT/'gba/source_ability_effects.bin';s=ROOT/'gba/source_ability_effects_data.s'
    bpath.write_bytes(bank)
    h.write_text('''#pragma once\n#include <stdint.h>\ntypedef struct{uint32_t off;uint16_t frame_bytes;uint8_t geom,reserved;int8_t ox,oy;uint16_t pad;} Ps4SourceAbilityPart;\ntypedef struct{uint16_t first_part;uint8_t part_count,frame_count;uint32_t holds_off;uint32_t source_table;} Ps4SourceAbilityAnim;\ntypedef struct{uint16_t first_anim;uint8_t anim_count,kind,sfx,palette_id;} Ps4SourceAbilityDesc;\nextern const uint8_t ps4_source_ability_bank[];\nextern const Ps4SourceAbilityPart ps4_source_ability_parts[];\nextern const Ps4SourceAbilityAnim ps4_source_ability_anims[];\n#define PS4_SOURCE_ABILITY_PALETTE_COUNT 50u\nextern const uint16_t ps4_source_ability_palettes[PS4_SOURCE_ABILITY_PALETTE_COUNT][16] __attribute__((aligned(4)));\nextern const Ps4SourceAbilityDesc ps4_source_tech[39];\nextern const Ps4SourceAbilityDesc ps4_source_skill[55];\n''')
    pal_ids={};palettes=[]
    for arr in abilities.values():
        for v in arr:
            if v is None:continue
            key=tuple(v[4])
            if key not in pal_ids:pal_ids[key]=len(palettes);palettes.append(key)
    if len(palettes)!=50:raise ValueError(f'unexpected ability palette count {len(palettes)}')
    def desc(v):
        if v is None:return '{0}'
        fa,na,k,fx,pal=v;return '{%du,%du,%du,%du,%du}'%(fa,na,k,fx,pal_ids[tuple(pal)])
    lines=['#include "source_ability_effects.h"','const Ps4SourceAbilityPart ps4_source_ability_parts[]={']
    lines += ['{%du,%du,%du,%du,%d,%d,0u},'%x for x in parts];lines+=['};','const Ps4SourceAbilityAnim ps4_source_ability_anims[]={']
    lines += ['{%du,%du,%du,%du,0x%06Xu},'%x for x in anims]
    lines += ['};','const uint16_t ps4_source_ability_palettes[PS4_SOURCE_ABILITY_PALETTE_COUNT][16] __attribute__((aligned(4)))={']
    lines += ['{%s},'%(','.join(str(x)+'u' for x in pal)) for pal in palettes]
    lines += ['};','const Ps4SourceAbilityDesc ps4_source_tech[39]={']
    lines += [desc(v)+',' for v in abilities['T']];lines+=['};','const Ps4SourceAbilityDesc ps4_source_skill[55]={']
    lines += [desc(v)+',' for v in abilities['S']];lines+=['};']
    c.write_text('\n'.join(lines)+'\n')
    s.write_text('.section .rodata.source_ability,"a",%progbits\n.balign 4\n.global ps4_source_ability_bank\nps4_source_ability_bank:\n.incbin "gba/source_ability_effects.bin"\n.balign 4\n')
    (ROOT/'source_ability_effects_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('source ability bank',len(bank),'bytes parts',len(parts),'anims',len(anims))
    for kind,arr in abilities.items():
        missing=[i for i in range(1,len(arr)) if not arr[i] or arr[i][2]==0]
        print(kind,'missing',missing)
if __name__=='__main__':main()
