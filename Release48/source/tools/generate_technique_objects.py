#!/usr/bin/env python3
"""Generate source-faithful PSIV technique BattleObj sheets for static recompiler.

Initial production slice:
- Foi ($01): generic cast ($400), orbit/release ($404), impact ($408)
- Res family ($18-$1D): healing cast ($494), target effect ($498)

Reads only the verified byte-perfect US PSIV ROM. No retired retired framework assets/code.
"""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from PIL import Image
from md_compression import nemesis_decompress
from generate_enemy_animation_frames import Rom
from generate_enemy_attack_frames import mapping, bounds, render, split, sheet, md_color

ROOT=Path(__file__).resolve().parent.parent
EXPECTED='511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a'
TRANS=(248,0,248)
PLC_TABLE=0x27E88A
PAL_SELECTOR=0x9AB0
PAL_WORDS=0x9AD6

# Ability IDs / PLCs / exact animation mapping tables.
FOI_ID=0x01; GIFOI_ID=0x02; FOI_PLC=0x50
FOI_CAST=0x25B62E
FOI_ORBIT=0x25B008
FOI_RELEASE=0x25B242
FOI_IMPACT=0x25AF70
NAFOI_ID=0x03; NAFOI_PLC=0x52
NAFOI_CHARGE=0x25B3D8
NAFOI_ATTACK=0x25B2B4
WAT_ID=0x04; WAT_PLC=0x53
WAT_RELEASE=0x25B720
TSU_ID=0x07; TSU_PLC=0x56
TSU_ATTACK=0x25B8A0
GITHU_ID=0x08
GITHU_CHILD=0x25B8AC
GITHU_PARENT=0x25B968
NATHU_ID=0x09
NATHU_CHILD=0x25B8C8
NATHU_PARENT=0x25B982
RES_IDS=(0x18,0x19,0x1A,0x1B,0x1C,0x1D)
RES_PLC=0x67
RES_CAST=0x25D39C
RES_EFFECT=0x25D542


def plc_chunks(rom:Rom,index:int,loader:int=1):
    # LoadPLC1 and LoadPLC2 have distinct source offset tables.
    table=PLC_TABLE if loader==1 else 0x27EA66
    rel=int.from_bytes(rom.read(table+index*2,2),'big',signed=True);p=table+rel
    count=int.from_bytes(rom.read(p,2),'big')+1
    ptrs=[int.from_bytes(rom.read(p+2+i*4,4),'big')&0xFFFFFF for i in range(count)]
    chunks=[nemesis_decompress(rom.tail(x)) for x in ptrs]
    return chunks,ptrs,[len(x)//32 for x in chunks]


def tech_palette(rom:Rom,tech_id:int):
    sel=rom.read(PAL_SELECTOR+tech_id-1,1)[0]
    p=PAL_WORDS+sel*8
    eff=[int.from_bytes(rom.read(p+i,2),'big') for i in range(0,8,2)]
    w=[int.from_bytes(rom.read(0x296300+i*2,2),'big') for i in range(16)];w[10:14]=eff
    return sel,eff,[md_color(x) for x in w]


def anim_table(rom:Rom,off:int):
    """Battle_AnimateSprite table; None mapping means source render-hidden frame."""
    count=int.from_bytes(rom.read(off,2),'big');base=off+2;maps=[];holds=[]
    for i in range(count):
        # 68000 indexed word offsets are signed. Some shared enemy tables
        # point backwards to existing frames (for example loc_25B23C).
        rel=int.from_bytes(rom.read(base+i*2,2),'big',signed=True);fp=base+rel
        holds.append(rom.read(fp,1)[0])
        # After duration, a negative byte is the source's hidden/no-mapping marker.
        maps.append(None if (rom.read(fp+1,1)[0]&0x80) else mapping(rom,fp+1))
    return maps,holds


def emit_anim(out:Path,name:str,art:bytes,maps,cols):
    visible=[m for m in maps if m is not None]
    if not visible: raise ValueError(f'{name}: no visible frames')
    bs=[bounds(m) for m in visible]
    u=(min(x[0] for x in bs),min(x[1] for x in bs),max(x[2] for x in bs),max(x[3] for x in bs))
    regs=split(u);parts=[]
    for j,r in enumerate(regs):
        ims=[]
        for m in maps:
            if m is None: ims.append(Image.new('RGB',(r[2]-r[0],r[3]-r[1]),TRANS))
            else: ims.append(render(art,m,cols,r))
        suffix='' if len(regs)==1 else f'_part{j+1}'
        parts.append(sheet(ims,r,out/f'{name}{suffix}.png'))
    return parts,list(u),[None if m is None else list(bounds(m)) for m in maps],[m is None for m in maps]


def one(out,rom,name,art,table,cols):
    maps,holds=anim_table(rom,table);parts,u,bs,hidden=emit_anim(out,name,art,maps,cols)
    return {'source_table':table,'holds':holds,'hidden':hidden,'parts':parts,'source_union':u,'frame_bounds':bs}

def one_regions(out,rom,name,art,table,cols,regions):
    """Emit one source animation using an explicit lossless GBA OBJ partition."""
    maps,holds=anim_table(rom,table);parts=[]
    for j,r in enumerate(regions):
        ims=[]
        for m in maps:
            if m is None: ims.append(Image.new('RGB',(r[2]-r[0],r[3]-r[1]),TRANS))
            else: ims.append(render(art,m,cols,r))
        parts.append(sheet(ims,r,out/f'{name}_part{j+1}.png'))
    visible=[bounds(m) for m in maps if m is not None]
    u=[min(x[0] for x in visible),min(x[1] for x in visible),max(x[2] for x in visible),max(x[3] for x in visible)]
    return {'source_table':table,'holds':holds,'hidden':[m is None for m in maps],'parts':parts,'source_union':u,'frame_bounds':[None if m is None else list(bounds(m)) for m in maps]}


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args()
    raw=a.rom.read_bytes();sha=hashlib.sha256(raw).hexdigest()
    if len(raw)!=0x300000 or sha!=EXPECTED: raise SystemExit(f'wrong PSIV ROM {len(raw)} {sha}')
    rom=Rom(raw);out=ROOT/'generated_precomp'/'technique';out.mkdir(parents=True,exist_ok=True)
    for p in out.glob('*'): p.unlink()
    desc={'source_rom_sha256':sha}

    # Foi: PLC50 banks map exactly to object VRAM destinations:
    # 9B80 cast=30, 9F40 impact=24, A240 orbit=19, A4A0 release=16.
    chunks,ptrs,counts=plc_chunks(rom,FOI_PLC)
    if counts!=[30,24,19,16]: raise ValueError(('Foi PLC50',counts))
    sel,words,cols=tech_palette(rom,FOI_ID)
    desc['foi']={
      'tech_ids':[FOI_ID,GIFOI_ID],'plc_index':FOI_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'palette_selector':sel,'palette_words':words,
      'cast':one(out,rom,'foi_cast',chunks[0],FOI_CAST,cols),
      'impact':one(out,rom,'foi_impact',chunks[1],FOI_IMPACT,cols),
      'orbit':one(out,rom,'foi_orbit',chunks[2],FOI_ORBIT,cols),
      'release':one(out,rom,'foi_release',chunks[3],FOI_RELEASE,cols),
      'timeline':{'cast_body_wait':30,'orbit_spawn_delay_after_body':0,'post_orbit_wait':8,'release_parent_advance_wait':2,'release_start_wait':6},
    }

    # Nafoi's $40C object is 32x136.  Its normal three-part decomposition is
    # already lossless; the only GBA scratch pressure comes from the generic
    # 48x49 $400 cast being rounded to one 64x64 OBJ.  Partition that cast as
    # 32x64 + 16x32 + 16x32 (3072 bytes) so cast + $40C remains below the
    # legal 8 KiB Mode-4 OBJ scratch window without changing source timing.
    chunks,ptrs,counts=plc_chunks(rom,NAFOI_PLC)
    if counts!=[30,30,6]: raise ValueError(('Nafoi PLC52',counts))
    sel,words,cols=tech_palette(rom,NAFOI_ID)
    cast_regions=[(-24,-24,8,25),(8,-24,24,8),(8,8,24,25)]
    desc['nafoi']={
      'tech_ids':[NAFOI_ID],'plc_index':NAFOI_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'palette_selector':sel,'palette_words':words,
      'cast':one_regions(out,rom,'nafoi_cast',chunks[0],FOI_CAST,cols,cast_regions),
      'attack':one(out,rom,'nafoi_attack',chunks[1],NAFOI_ATTACK,cols),
      'charge':one(out,rom,'nafoi_charge',chunks[2],NAFOI_CHARGE,cols),
      'timeline':{'cast_body_wait':30,'post_charge_wait':8,'parent_advance_wait':2},
    }

    # Wat uses the same common cast/orbit/impact object family with its own
    # CRAM selector and a different 35-tile release bank / release wait.
    chunks,ptrs,counts=plc_chunks(rom,WAT_PLC)
    if counts!=[30,24,19,35]: raise ValueError(('Wat PLC53',counts))
    sel,words,cols=tech_palette(rom,WAT_ID)
    desc['wat']={
      'tech_ids':[WAT_ID],'plc_index':WAT_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'palette_selector':sel,'palette_words':words,
      'cast':one(out,rom,'wat_cast',chunks[0],FOI_CAST,cols),
      'impact':one(out,rom,'wat_impact',chunks[1],FOI_IMPACT,cols),
      'orbit':one(out,rom,'wat_orbit',chunks[2],FOI_ORBIT,cols),
      'release':one(out,rom,'wat_release',chunks[3],WAT_RELEASE,cols),
      'timeline':{'cast_body_wait':30,'tech_obj_spawn_delay':12,'post_orbit_wait':8,'release_parent_advance_wait':2,'release_start_wait':9},
    }

    # Tsu starts a separate technique family.  PLC56 is shared by Tsu/Githu/
    # Nathu, but the base Tsu object ($420) consumes the second bank directly
    # at VDP $9F40 and animates loc_25B8A0.  Preserve the original non-zero
    # mapping centre rather than recentering the effect around the target.
    chunks,ptrs,counts=plc_chunks(rom,TSU_PLC)
    if counts!=[30,68,80]: raise ValueError(('Tsu PLC56',counts))
    sel,words,cols=tech_palette(rom,TSU_ID)
    desc['tsu']={
      'tech_ids':[TSU_ID],'plc_index':TSU_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'palette_selector':sel,'palette_words':words,
      'cast':one(out,rom,'tsu_cast',chunks[0],FOI_CAST,cols),
      'attack':one(out,rom,'tsu_attack',chunks[1],TSU_ATTACK,cols),
      'timeline':{'cast_body_wait':30,'parent_advance_wait':2},
    }

    # Githu shares PLC56 with Tsu but runs two source BattleObjs at once:
    # $41C over bank 1 and the $424 controller/visual layer over bank 2.
    # Both source unions lower to 32x64, so cast + both layers fit the static
    # renderer's exact 8 KiB Mode-4 OBJ scratch window without approximation.
    sel,words,cols=tech_palette(rom,GITHU_ID)
    desc['githu']={
      'tech_ids':[GITHU_ID],'plc_index':TSU_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'palette_selector':sel,'palette_words':words,
      'cast':one(out,rom,'githu_cast',chunks[0],FOI_CAST,cols),
      'child':one(out,rom,'githu_child',chunks[1],GITHU_CHILD,cols),
      'parent':one(out,rom,'githu_parent',chunks[2],GITHU_PARENT,cols),
      'timeline':{'cast_body_wait':30,'parent_hidden_wait':2},
    }

    # Nathu is the larger two-layer sibling.  Tight source-region partitions
    # keep the simultaneous cast + $41C child + $434 parent at 7680 bytes:
    # 3072 + 2048 + 2560.  No source pixel or animation frame is discarded.
    sel,words,cols=tech_palette(rom,NATHU_ID)
    cast_regions=[(-24,-24,8,25),(8,-24,24,8),(8,8,24,25)]
    parent_regions=[(-4,-2,28,54),(28,-2,32,30),(28,30,32,54)]
    desc['nathu']={
      'tech_ids':[NATHU_ID],'plc_index':TSU_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'palette_selector':sel,'palette_words':words,
      'cast':one_regions(out,rom,'nathu_cast',chunks[0],FOI_CAST,cols,cast_regions),
      'child':one(out,rom,'nathu_child',chunks[1],NATHU_CHILD,cols),
      'parent':one_regions(out,rom,'nathu_parent',chunks[2],NATHU_PARENT,cols,parent_regions),
      'timeline':{'cast_body_wait':30,'parent_hidden_wait':2},
    }

    # Healing family shares object code/PLC geometry; source palette selector differs
    # by tier. Generate the three palette variants so Res/Gires/Nares and Sar tiers
    # can retain their actual CRAM colors without runtime recoloring.
    chunks,ptrs,counts=plc_chunks(rom,RES_PLC)
    if counts!=[50,72]: raise ValueError(('Res PLC67',counts))
    variants=[]
    for tid in (0x18,0x19,0x1A):
        sel,words,cols=tech_palette(rom,tid)
        suffix={0x18:'res',0x19:'gires',0x1A:'nares'}[tid]
        variants.append({
          'name':suffix,'tech_ids':[tid,tid+3],'palette_selector':sel,'palette_words':words,
          'cast':one(out,rom,f'{suffix}_heal_cast',chunks[0],RES_CAST,cols),
          'effect':one(out,rom,f'{suffix}_heal_effect',chunks[1],RES_EFFECT,cols),
        })
    desc['healing']={
      'tech_ids':list(RES_IDS),'plc_index':RES_PLC,'art_sources':ptrs,'plc_tile_counts':counts,
      'variants':variants,'timeline':{'cast_body_wait':30,'effect_y':0x108},
    }
    (out/'manifest.json').write_text(json.dumps(desc,indent=2)+'\n')
    print('wrote technique objects',out)
    print('Healing',counts,'variants',[(v['name'],v['palette_selector']) for v in variants])
if __name__=='__main__': main()
