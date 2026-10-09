#!/usr/bin/env python3
"""Generate PSIV party physical-attack BattleObj assets for the static port.

This generator consumes only the verified US PSIV ROM.  It currently lowers:
- Alys/Kyra slasher trace/throw objects used by CharAttack_AlysKyra;
- Demi's Impacter projectile used by CharAttack_Demi/BattleObj_Shot.

The output is packed into precomp_assets by generate_precompiled_visuals.py.
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

# Source addresses from the byte-perfect PSIV listing.
PLC_TABLE=0x27E88A
ALYS_PLC=0x77
ALYS_TRACE_A=0x25FDCC
ALYS_TRACE_B=0x25FDDE
ALYS_THROW=0x25FDF0
ALYS_SWEEP=0x2608EE
ALYS_TRAIL=0x260918
DEMI_IMPACTER_ART=(0x244CF2,0x244054)
DEMI_IMPACTER_ANIM=0x25BA40
# Skill_Vortex loads PLC $92, then BattleObj_Vortex instantiates the exact
# slasher-family mapping tables below.  These must be rendered from the skill
# PLC, not from Alys's physical-attack PLC $77.
VORTEX_ID=0x06
VORTEX_PLC=0x92
VORTEX_TRACE_A=0x25FDCC
VORTEX_TRACE_B=0x25FDDE
VORTEX_THROW=0x25FDF0
VORTEX_RETURN=0x25E58C
VORTEX_IMPACT=0x25E5A8
# Early-game skill BattleObjs used by the initial Chaz/Alys/Hahn party.
EARTH_ID=0x1F
EARTH_PLC=0xAB
EARTH_MAIN=0x25E54E
EARTH_SUCCESS=0x25D0C4
VISION_ID=0x2F
VISION_PLC=0xBB
VISION_EFFECT=0x25D6C0
SKILL_PALETTE_SELECT=0x9CCA
SKILL_PALETTE_WORDS=0x9D32

ALYS_RIGHT=((0x013C,0x0103),(0x0139,0x010B),(0x0146,0x0105),(0x0158,0x0101),(0x0170,0x0100),(0x016F,0x010A),(0x017B,0x0103),(0x018C,0x00FC),(0x019F,0x00FA),(0x01A5,0x0104),(0x01AC,0x00FC),(0x01B3,0x00ED),(0x0260,0x00D8))
ALYS_LEFT=((0x0104,0x0103),(0x0107,0x010B),(0x00FA,0x0105),(0x00E8,0x0101),(0x00D0,0x0100),(0x00D1,0x010A),(0x00C5,0x0103),(0x00B4,0x00FC),(0x00A1,0x00FA),(0x009B,0x0104),(0x0094,0x00FC),(0x008D,0x00ED),(0xFFE0,0x00D8))


def skill_palette(rom:Rom,skill_id:int):
    """Character_DoSkill palette override at loc_9CCA/loc_9D32."""
    sel=rom.read(SKILL_PALETTE_SELECT+skill_id,1)[0]
    words=[int.from_bytes(rom.read(0x296300+i*2,2),'big') for i in range(16)]
    if sel:
        eff=[int.from_bytes(rom.read(SKILL_PALETTE_WORDS+sel*8+i,2),'big') for i in range(0,8,2)]
        words[10:14]=eff
    else:
        eff=[]
    return sel,eff,[md_color(x) for x in words]

def cols_with_weapon(words4,base_words):
    w=list(base_words);w[10:14]=list(words4)
    return [md_color(x) for x in w]

def custom_slasher_table(rom:Rom,off:int):
    """Battle_AnimateSlasher: word count, then byte unk, byte hold, word map-offset."""
    count=int.from_bytes(rom.read(off,2),'big')
    base=off+4
    maps=[];holds=[]
    p=off+2
    for _ in range(count):
        e=rom.read(p,4);holds.append(e[1]);rel=int.from_bytes(e[2:4],'big');maps.append(mapping(rom,base+rel));p+=4
    return maps,holds

def standard_anim_table(rom:Rom,off:int):
    """Battle_AnimateSprite: word count then word offsets; pointed frame starts hold byte."""
    count=int.from_bytes(rom.read(off,2),'big')
    base=off+2;maps=[];holds=[]
    for i in range(count):
        rel=int.from_bytes(rom.read(base+i*2,2),'big');fp=base+rel;holds.append(rom.read(fp,1)[0]);maps.append(mapping(rom,fp+1))
    return maps,holds

def emit(out:Path,name:str,art:bytes,maps,cols):
    bs=[bounds(m) for m in maps];u=(min(x[0] for x in bs),min(x[1] for x in bs),max(x[2] for x in bs),max(x[3] for x in bs));parts=[]
    regs=split(u)
    for j,r in enumerate(regs):
        ims=[render(art,m,cols,r) for m in maps]
        suffix='' if len(regs)==1 else f'_part{j+1}'
        parts.append(sheet(ims,r,out/f'{name}{suffix}.png'))
    return parts,list(u),[list(x) for x in bs]

def plc_chunks(rom:Rom,index:int):
    rel=int.from_bytes(rom.read(PLC_TABLE+index*2,2),'big');p=PLC_TABLE+rel;count=int.from_bytes(rom.read(p,2),'big')+1
    ptrs=[int.from_bytes(rom.read(p+2+i*4,4),'big')&0xFFFFFF for i in range(count)]
    chunks=[nemesis_decompress(rom.tail(x)) for x in ptrs]
    return chunks,ptrs,[len(x)//32 for x in chunks]

def plc_art(rom:Rom,index:int):
    chunks,ptrs,counts=plc_chunks(rom,index)
    return b''.join(chunks),ptrs,counts

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args()
    raw=a.rom.read_bytes();sha=hashlib.sha256(raw).hexdigest()
    if len(raw)!=0x300000 or sha!=EXPECTED:raise SystemExit(f'wrong PSIV ROM {len(raw)} {sha}')
    rom=Rom(raw);base_words=tuple(int.from_bytes(rom.read(0x296300+i*2,2),'big') for i in range(16));out=ROOT/'generated_precomp'/'party_attack';out.mkdir(parents=True,exist_ok=True)
    for p in out.glob('*'):p.unlink()
    desc={'source_rom_sha256':sha}

    # Alys/Kyra slasher family. Character_Attack loads weapon PLC $77 before
    # CharAttack_AlysKyra. Attack2 then loads PLC $C3 over tiles 0..$83 only;
    # sweep/trail mappings deliberately use the preserved PLC $77 tail at $84+.
    alys_art,alys_sources,alys_tile_counts=plc_art(rom,ALYS_PLC);alys_cols=[md_color(x) for x in base_words]
    am={}
    for name,off in (('trace_a',ALYS_TRACE_A),('trace_b',ALYS_TRACE_B),('throw',ALYS_THROW)):
        maps,holds=custom_slasher_table(rom,off);parts,u,bs=emit(out,f'alys_slasher_{name}',alys_art,maps,alys_cols)
        am[name]={'source_table':off,'holds':holds,'parts':parts,'source_union':u,'frame_bounds':bs}
    for name,off in (('sweep',ALYS_SWEEP),('trail',ALYS_TRAIL)):
        maps,holds=standard_anim_table(rom,off);parts,u,bs=emit(out,f'alys_slasher_{name}',alys_art,maps,alys_cols)
        am[name]={'source_table':off,'holds':holds,'parts':parts,'source_union':u,'frame_bounds':bs}
    desc['alys_slasher']={
        'family_items':[3,9,0x14,0x23,0x5D,0x65],'weapon_type':2,'plc_index':ALYS_PLC,'art_sources':alys_sources,'plc_tile_counts':alys_tile_counts,'art_tiles':len(alys_art)//32,
        'body_windup':30,'right_coords':[list(x) for x in ALYS_RIGHT],'left_coords':[list(x) for x in ALYS_LEFT],
        'sweep':{'right_x_start':0x320,'left_x_start':-0xE0,'x_step':0x10,'y':0xD8,'updates':64},'animations':am
    }

    # Vortex is visually related to the thrown slasher machinery, but PSIV
    # loads the skill-specific PLC $92 before BattleObj_Vortex.  Render these
    # same source mapping tables from that PLC so the native port does not
    # accidentally show Alys's physical-attack art for the skill.
    vortex_chunks,vortex_sources,vortex_counts=plc_chunks(rom,VORTEX_PLC);vortex_art=b''.join(vortex_chunks)
    vsel,vwords,vcols=skill_palette(rom,VORTEX_ID)
    vm={}
    for name,off in (('trace_a',VORTEX_TRACE_A),('trace_b',VORTEX_TRACE_B),('throw',VORTEX_THROW)):
        maps,holds=custom_slasher_table(rom,off);parts,u,bs=emit(out,f'vortex_{name}',vortex_art,maps,vcols)
        vm[name]={'source_table':off,'holds':holds,'parts':parts,'source_union':u,'frame_bounds':bs}
    # BattleObj_VortexChild changes VRAM destination to $AC00 after the throw,
    # then spawns three AbilityAnim children at $AD20.  PLC $92 is five
    # sequential chunks loaded from skill base $9C40; the cumulative tile
    # counts place chunk 4 exactly at $AC00 and chunk 5 exactly at $AD20.
    # Render these later controller phases from those exact chunks rather than
    # the concatenated PLC base used by the initial slasher mappings.
    if vortex_counts[:4] != [31,26,69,9] or len(vortex_chunks) < 5:
        raise ValueError(('Vortex PLC 92 layout',vortex_counts))
    rmaps,rholds=standard_anim_table(rom,VORTEX_RETURN);rparts,ru,rbs=emit(out,'vortex_return',vortex_chunks[3],rmaps,vcols)
    vm['return']={'source_table':VORTEX_RETURN,'holds':rholds,'parts':rparts,'source_union':ru,'frame_bounds':rbs}
    imaps,iholds=standard_anim_table(rom,VORTEX_IMPACT);iparts,iu,ibs=emit(out,'vortex_impact',vortex_chunks[4],imaps,vcols)
    vm['impact']={'source_table':VORTEX_IMPACT,'holds':iholds,'parts':iparts,'source_union':iu,'frame_bounds':ibs}
    desc['vortex']={
        'skill_id':VORTEX_ID,'plc_index':VORTEX_PLC,'art_sources':vortex_sources,'plc_tile_counts':vortex_counts,'art_tiles':len(vortex_art)//32,
        'palette_selector':vsel,'palette_words':vwords,'root_wait':0x1E,'return_wait':0x14,'impact_finish_wait':0x11,'finish_wait':0x20,
        'return_x_start':0x1C0,'return_y':0xE8,'return_x_step':-0x10,'impact_x_offset':-0x28,'impact_y':0xD8,'impact_delays':[5,0x16,0x27],'impact_y_offsets':[0,-0x0A,-0x14],
        'right_coords':[list(x) for x in ALYS_RIGHT[:12]],'animations':vm
    }

    # Earth: Character_DoSkill loads PLC $AB at $9B80.  The first 37-tile
    # bank feeds Map_Earth2 at $9B80, while the following 23-tile bank begins
    # at $A020 and feeds Map_Earth.  Preserve both source animation tables.
    earth_chunks,earth_sources,earth_counts=plc_chunks(rom,EARTH_PLC)
    if earth_counts!=[37,23]: raise ValueError(('Earth PLC AB',earth_counts))
    esel,ewords,ecols=skill_palette(rom,EARTH_ID)
    emaps,eholds=standard_anim_table(rom,EARTH_MAIN);eparts,eu,ebs=emit(out,'earth_main',earth_chunks[1],emaps,ecols)
    smaps,sholds=standard_anim_table(rom,EARTH_SUCCESS);sparts,su,sbs=emit(out,'earth_success',earth_chunks[0],smaps,ecols)
    desc['earth']={
        'skill_id':EARTH_ID,'plc_index':EARTH_PLC,'art_sources':earth_sources,'plc_tile_counts':earth_counts,
        'palette_selector':esel,'palette_words':ewords,'root_wait':0x3C,'success_wait':0x64,
        'main':{'source_table':EARTH_MAIN,'holds':eholds,'parts':eparts,'source_union':eu,'frame_bounds':ebs},
        'success':{'source_table':EARTH_SUCCESS,'holds':sholds,'parts':sparts,'source_union':su,'frame_bounds':sbs},
    }

    # Vision: PLC $BB's first bank is loaded at $9B80 and the root object
    # instantiates two copies of loc_25D6C0 around every successful party
    # target (x-8/x+8, y=$100) for an $50-update controller lifetime.
    vision_chunks,vision_sources,vision_counts=plc_chunks(rom,VISION_PLC)
    if vision_counts!=[55,26]: raise ValueError(('Vision PLC BB',vision_counts))
    vsel2,vwords2,vcols2=skill_palette(rom,VISION_ID)
    vmaps2,vholds2=standard_anim_table(rom,VISION_EFFECT);vparts2,vu2,vbs2=emit(out,'vision_effect',vision_chunks[0],vmaps2,vcols2)
    desc['vision']={
        'skill_id':VISION_ID,'plc_index':VISION_PLC,'art_sources':vision_sources,'plc_tile_counts':vision_counts,
        'palette_selector':vsel2,'palette_words':vwords2,'root_wait':0x50,
        'effect':{'source_table':VISION_EFFECT,'holds':vholds2,'parts':vparts2,'source_union':vu2,'frame_bounds':vbs2},
    }

    # Demi Impacter: PLC $83 = streams 244CF2 + 244054, weapon-palette index 1.
    demi_art=b''.join(nemesis_decompress(rom.tail(x)) for x in DEMI_IMPACTER_ART);demi_cols=cols_with_weapon((0x06CE,0x02AC,0x008A,0x0068),base_words)
    maps,holds=standard_anim_table(rom,DEMI_IMPACTER_ANIM);parts,u,bs=emit(out,'demi_impacter_shot',demi_art,maps,demi_cols)
    desc['demi_impacter']={
        'item_id':0x2B,'weapon_id':0x16,'plc_index':0x83,'art_sources':list(DEMI_IMPACTER_ART),'art_tiles':len(demi_art)//32,
        'body_windup':30,'shot_delay':8,'source_table':DEMI_IMPACTER_ANIM,'holds':holds,'parts':parts,'source_union':u,'frame_bounds':bs,
        'impact_reuses':'jurafa_micro_impact'
    }
    (out/'manifest.json').write_text(json.dumps(desc,indent=2)+'\n')
    print('wrote party attack objects',out)
    print('Alys tiles',len(alys_art)//32,'Demi tiles',len(demi_art)//32,'Demi holds',holds)
if __name__=='__main__':main()
