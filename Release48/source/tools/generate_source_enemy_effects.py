#!/usr/bin/env python3
"""Generate source-only PSIV enemy special-attack presentation data.

The checked-in pair specification was mechanically derived from the pinned
squidfeatures/ps4disasm lineage.  This build step needs only the verified retail
ROM: it independently decodes every referenced mapping/PLC and emits GBA-ready
4bpp OBJ frames.  No generic/fabricated enemy effect is generated.
"""
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from md_compression import nemesis_decompress
from generate_enemy_animation_frames import Rom,enemy_record,assembled_enemy_tiles
from generate_enemy_attack_frames import mapping,records,bounds,split,legal,palette_words
from generate_technique_objects import plc_chunks
from source_enemy_metadata_tables import METADATA
ROOT=Path(__file__).resolve().parent.parent
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
SPEC=Path(__file__).with_name('source_enemy_effect_specs.json')
KIND={'generated':1,'source_noop':2,'body_dblslash':3,'rimit_screen':4,'earthquake_screen':5,'existing':6}
TARGET={'self':0,'victim':1,'all_party':2}

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

def mapbytes(rom:Rom,addr:int):
    n=rom.read(addr,1)[0]+1
    if not 1<=n<=40:raise ValueError(f'bad mapping {addr:06X} count={n}')
    return rom.read(addr,1+n*6)

def art_pool(rom:Rom,eid:int,seq:dict):
    if seq.get('source_nemesis') is not None:
        return nemesis_decompress(rom.tail(int(seq['source_nemesis'])))
    if seq['art_kind']=='plc':
        chunks=[]
        for idx in seq['art_plcs']:
            cs,_,_=plc_chunks(rom,int(idx));chunks.extend(cs)
        if not chunks:raise ValueError(f'{eid:02X}: no PLC art')
        return b''.join(chunks)
    art,_,_,_=assembled_enemy_tiles(rom,eid)
    return art

def render_indices(art:bytes,m:bytes,reg,base:int):
    x0,y0,x1,y1=reg;w=x1-x0;h=y1-y0;pix=bytearray(w*h);tc=len(art)//32
    for x,y,tw,th,p in records(m):
        bt=(p&0x7ff)-base;hf=bool(p&0x0800);vf=bool(p&0x1000)
        for tx in range(tw):
            for ty in range(th):
                stx=tw-1-tx if hf else tx;sty=th-1-ty if vf else ty;ti=bt+stx*th+sty
                if not 0<=ti<tc:raise ValueError(f'tile {ti} outside {tc}; base={base}')
                tile=art[ti*32:(ti+1)*32]
                for py in range(8):
                    sy=7-py if vf else py;wy=y+ty*8+py
                    if not y0<=wy<y1:continue
                    for px in range(8):
                        sx=7-px if hf else px;wx=x+tx*8+px
                        if not x0<=wx<x1:continue
                        q=tile[sy*4+(sx>>1)];ci=(q>>4) if not(sx&1) else q&15
                        if ci:pix[(wy-y0)*w+(wx-x0)]=ci
    return pix,w,h

def pack_canvas(pix,sw,sh,cw,ch):
    px=(cw-sw)//2;py=(ch-sh)//2;out=bytearray(cw*ch//2);o=0
    for ty in range(0,ch,8):
      for tx in range(0,cw,8):
       for y in range(8):
        for x in range(0,8,2):
          def get(xx):
            sx=tx+xx-px;sy=ty+y-py
            return pix[sy*sw+sx] if 0<=sx<sw and 0<=sy<sh else 0
          out[o]=get(x)|(get(x+1)<<4);o+=1
    return bytes(out)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args()
    raw=a.rom.read_bytes();sha=hashlib.sha256(raw).hexdigest()
    if len(raw)!=0x300000 or sha!=EXPECTED:raise SystemExit(f'wrong PSIV ROM {len(raw)} {sha}')
    spec=json.loads(SPEC.read_text())
    if spec.get('source_rom_sha256')!=EXPECTED or len(spec.get('pairs',[]))!=234:raise SystemExit('bad source enemy specification')
    rom=Rom(raw);ability=(ROOT/'gba/source_ability_effects.bin').read_bytes();bank=bytearray();blob_cache={};parts=[];anims=[];descs=[];source_sfx_ids=[];source_sfx_off=[0];lookup=[0xFFFF]*(153*113);manifest=[]
    def put(b:bytes,align=4):
        key=(align,bytes(b))
        if key in blob_cache:return blob_cache[key]
        while len(bank)%align:bank.append(0)
        off=len(bank);bank.extend(b);blob_cache[key]=off;return off
    for rec in spec['pairs']:
        eid=int(rec['enemy']);aid=int(rec['ability']);kind=KIND[rec['kind']];first_anim=len(anims);seq_out=[]
        for seq in rec.get('sequences',[]):
            if int(seq['source_table']) in METADATA:
                anims.append((len(parts),0,1,put(bytes([1])),int(seq['source_table'])))
                seq_out.append({'source_table':f'{int(seq["source_table"]):06X}','controller_data':METADATA[int(seq['source_table'])],'frames':1,'parts':0})
                continue
            art=art_pool(rom,eid,seq);frames=[]
            for fa in seq['frames']:
                frames.append(None if fa is None else mapbytes(rom,int(fa)))
            vis=[m for m in frames if m is not None]
            if not vis:continue
            bs=[bounds(m) for m in vis];u=(min(x[0] for x in bs),min(x[1] for x in bs),max(x[2] for x in bs),max(x[3] for x in bs));regs=split(u)
            live=0;staged=[]
            for r in regs:
                sw,sh=r[2]-r[0],r[3]-r[1];cw,ch=legal(sw,sh);frame_bytes=cw*ch//2;live+=frame_bytes;payload=[]
                cx,cy=(r[0]+r[2])//2,(r[1]+r[3])//2;geom,ox,oy=obj_part_meta(cw,ch,cx,cy)
                for m in frames:
                    if m is None:payload.append(bytes(frame_bytes))
                    else:
                        pix,pw,ph=render_indices(art,m,(ox,oy,ox+cw,oy+ch),int(seq['tile_base']));payload.append(pack_canvas(pix,cw,ch,cw,ch))
                cx,cy=(r[0]+r[2])//2,(r[1]+r[3])//2;geom,ox,oy=obj_part_meta(cw,ch,cx,cy);staged.append((b''.join(payload),frame_bytes,geom,0,ox,oy))
            nt=5 if rec['target']=='all_party' else 1
            if live>0x4000 or len(regs)*nt>48:
                continue
            first_part=len(parts)
            for dat,frame_bytes,geom,reserved,ox,oy in staged:
                at=ability.find(dat)
                while at>=0 and at%4:at=ability.find(dat,at+1)
                off,reserved=(at,1) if at>=0 else (put(dat),0)
                parts.append((off,frame_bytes,geom,reserved,ox,oy))
            holds=bytes(max(1,int(x)&255) for x in seq['holds']);ho=put(holds)
            anims.append((first_part,len(regs),len(frames),ho,int(seq['source_table'])))
            seq_out.append({'source_table':f'{int(seq["source_table"]):06X}','frames':len(frames),'parts':len(regs),'art_kind':seq['art_kind'],'tile_base':int(seq['tile_base']),'live_bytes':live})
        anim_count=len(anims)-first_anim
        if kind==KIND['generated'] and not anim_count:raise ValueError(f'generated pair {eid:02X}/{aid:02X} has no animation')
        pal=[md2gba(x) for x in palette_words(rom,eid)]
        primary=int(rec.get('primary_sfx',0))&255
        pair_sfx=[]
        for sv in rec.get('source_sfx_ids',[]):
            sv=int(sv)&255
            if sv and sv not in pair_sfx:pair_sfx.append(sv)
        if pair_sfx and primary!=pair_sfx[0]:raise ValueError(f'{eid:02X}/{aid:02X}: primary/source SFX mismatch')
        source_sfx_ids.extend(pair_sfx);source_sfx_off.append(len(source_sfx_ids))
        desc_index=len(descs);descs.append((first_anim,anim_count,kind,TARGET[rec['target']],primary,pal,eid,aid))
        lookup[eid*113+aid]=desc_index
        manifest.append({'enemy':f'{eid:02X}','ability':f'{aid:02X}','kind':rec['kind'],'target':rec['target'],'range':rec['range'],'roots':[f'{int(x):03X}' for x in rec.get('roots',[])],'plcs':rec.get('plcs',[]),'primary_sfx':primary,'source_sfx_names':rec.get('source_sfx_names',[]),'source_sfx_ids':pair_sfx,'animations':seq_out})
    if len(descs)!=234 or sum(x!=0xFFFF for x in lookup)!=234:raise ValueError('pair coverage')
    # Emit data.
    (ROOT/'gba/source_enemy_effects.bin').write_bytes(bank)
    (ROOT/'gba/source_enemy_effects.h').write_text('''#pragma once\n#include <stdint.h>\ntypedef struct{uint32_t off;uint16_t frame_bytes;uint8_t geom,reserved;int8_t ox,oy;uint16_t pad;} Ps4SourceEnemyPart;\ntypedef struct{uint16_t first_part;uint8_t part_count,frame_count;uint32_t holds_off;uint32_t source_table;} Ps4SourceEnemyAnim;\ntypedef struct{uint16_t first_anim;uint8_t anim_count,kind,target_mode,primary_sfx,palette_id,enemy_id,ability_id;} Ps4SourceEnemyDesc;\ntypedef struct{uint8_t ability_id,desc_index;} Ps4SourceEnemyLookupEntry;\nextern const uint8_t ps4_source_enemy_bank[];\nextern const Ps4SourceEnemyPart ps4_source_enemy_parts[];\nextern const Ps4SourceEnemyAnim ps4_source_enemy_anims[];\nextern const Ps4SourceEnemyDesc ps4_source_enemy_descs[234];\n#define PS4_SOURCE_ENEMY_PALETTE_COUNT 108u\nextern const uint16_t ps4_source_enemy_palettes[PS4_SOURCE_ENEMY_PALETTE_COUNT][16] __attribute__((aligned(4)));\nextern const uint16_t ps4_source_enemy_row_off[154];\nextern const Ps4SourceEnemyLookupEntry ps4_source_enemy_lookup_entries[234];\nextern const uint16_t ps4_source_enemy_sfx_off[235];\nextern const uint8_t ps4_source_enemy_sfx_ids[];\nconst Ps4SourceEnemyDesc* ps4_source_enemy_find(uint8_t enemy_id,uint8_t ability_id);\nint ps4_source_enemy_has_sfx(const Ps4SourceEnemyDesc* d,uint8_t sfx);\n#define PS4_SOURCE_ENEMY_KIND_GENERATED 1u\n#define PS4_SOURCE_ENEMY_KIND_NOOP 2u\n#define PS4_SOURCE_ENEMY_KIND_BODY_DBLSLASH 3u\n#define PS4_SOURCE_ENEMY_KIND_RIMIT_SCREEN 4u\n#define PS4_SOURCE_ENEMY_KIND_EARTHQUAKE_SCREEN 5u\n#define PS4_SOURCE_ENEMY_KIND_EXISTING 6u\n#define PS4_SOURCE_ENEMY_TARGET_SELF 0u\n#define PS4_SOURCE_ENEMY_TARGET_VICTIM 1u\n#define PS4_SOURCE_ENEMY_TARGET_ALL_PARTY 2u\n''')
    def fmt_part(x):return '{%du,%du,%du,%du,%d,%d,0u}'%x
    def fmt_anim(x):return '{%du,%du,%du,%du,0x%06Xu}'%x
    # Compact aligned palette bank and sparse per-enemy dispatch.
    pal_ids={};palettes=[]
    for d in descs:
        key=tuple(d[5])
        if key not in pal_ids:pal_ids[key]=len(palettes);palettes.append(key)
    if len(palettes)!=108:raise ValueError(f'unexpected enemy palette count {len(palettes)}')
    rows=[[] for _ in range(153)]
    for i,d in enumerate(descs):
        eid,aid=d[6],d[7]
        if i>255:raise ValueError('enemy descriptor index no longer fits compact lookup')
        rows[eid].append((aid,i))
    for row in rows:row.sort()
    row_off=[0];entries=[]
    for row in rows:entries.extend(row);row_off.append(len(entries))
    if len(entries)!=234:raise ValueError('compact lookup coverage')
    lines=['#include "source_enemy_effects.h"','extern const uint8_t ps4_source_enemy_bank[];','const Ps4SourceEnemyPart ps4_source_enemy_parts[]={']+[fmt_part(x)+',' for x in parts]+['};','const Ps4SourceEnemyAnim ps4_source_enemy_anims[]={']+[fmt_anim(x)+',' for x in anims]+['};','const uint16_t ps4_source_enemy_palettes[PS4_SOURCE_ENEMY_PALETTE_COUNT][16] __attribute__((aligned(4)))={']
    lines += ['{%s},'%(','.join(str(x)+'u' for x in pal)) for pal in palettes]
    lines += ['};','const Ps4SourceEnemyDesc ps4_source_enemy_descs[234]={']
    for first,count,kind,target,sfx,pal,eid,aid in descs:
        lines.append('{%du,%du,%du,%du,%du,%du,%du,%du},'%(first,count,kind,target,sfx,pal_ids[tuple(pal)],eid,aid))
    lines += ['};','const uint16_t ps4_source_enemy_row_off[154]={']
    for i in range(0,len(row_off),16):lines.append(','.join(str(x)+'u' for x in row_off[i:i+16])+',')
    lines += ['};','const Ps4SourceEnemyLookupEntry ps4_source_enemy_lookup_entries[234]={']
    for i in range(0,len(entries),12):lines.append(','.join('{%du,%du}'%x for x in entries[i:i+12])+',')
    lines += ['};','const uint16_t ps4_source_enemy_sfx_off[235]={']
    for i in range(0,len(source_sfx_off),16):lines.append(','.join(str(x)+'u' for x in source_sfx_off[i:i+16])+',')
    lines += ['};','const uint8_t ps4_source_enemy_sfx_ids[]={']
    for i in range(0,len(source_sfx_ids),24):lines.append(','.join(str(x)+'u' for x in source_sfx_ids[i:i+24])+',')
    lines += ['};','const Ps4SourceEnemyDesc* ps4_source_enemy_find(uint8_t e,uint8_t a){if(e>=153u||a>=113u)return 0;unsigned lo=ps4_source_enemy_row_off[e],hi=ps4_source_enemy_row_off[(unsigned)e+1u];for(unsigned i=lo;i<hi;i++){Ps4SourceEnemyLookupEntry q=ps4_source_enemy_lookup_entries[i];if(q.ability_id==a)return &ps4_source_enemy_descs[q.desc_index];if(q.ability_id>a)break;}return 0;}','int ps4_source_enemy_has_sfx(const Ps4SourceEnemyDesc* d,uint8_t sfx){if(!d||!sfx)return 0;unsigned di=(unsigned)(d-ps4_source_enemy_descs);if(di>=234u)return 0;unsigned lo=ps4_source_enemy_sfx_off[di],hi=ps4_source_enemy_sfx_off[di+1u];for(unsigned i=lo;i<hi;i++)if(ps4_source_enemy_sfx_ids[i]==sfx)return 1;return 0;}','']
    (ROOT/'gba/source_enemy_effects.c').write_text('\n'.join(lines))
    (ROOT/'gba/source_enemy_effects_data.s').write_text('.section .rodata.ps4_source_enemy,"a",%progbits\n.balign 4\n.global ps4_source_enemy_bank\nps4_source_enemy_bank:\n.incbin "gba/source_enemy_effects.bin"\n')
    report={'source_rom_sha256':sha,'pairs':len(descs),'generated_pairs':sum(d[2]==1 for d in descs),'source_noop_pairs':sum(d[2]==2 for d in descs),'existing_bespoke_pairs':sum(d[2]==6 for d in descs),'animations':len(anims),'parts':len(parts),'bank_bytes':len(bank),'unique_payloads':len(blob_cache),'manifest':manifest}
    (ROOT/'PS4-PSCSTYLE-v0.9-enemy-source-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS source enemy effects:',len(descs),'pairs',len(anims),'anims',len(parts),'parts',len(bank),'bytes')
if __name__=='__main__':main()
