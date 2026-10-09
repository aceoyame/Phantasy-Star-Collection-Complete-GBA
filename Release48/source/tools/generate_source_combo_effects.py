#!/usr/bin/env python3
"""Generate source-backed PSIV combination-attack visuals from retail PLC/mappings.
R20 deliberately emits only mapping tables whose referenced tiles are actually
present in the combo's Character_DoCombo PLC package. Controller-only combos are
marked special for runtime lowering rather than receiving invented fallback art.
"""
from pathlib import Path
import argparse,json
from generate_enemy_animation_frames import Rom
from generate_technique_objects import plc_chunks,anim_table
from generate_enemy_attack_frames import bounds,split,legal,records,mapping
from source_combo_art_sources import DESTINATIONS,PHYSICAL,SLASHER
from generate_source_ability_effects import render_indices,pack_canvas,obj_part_meta,md2gba
ROOT=Path(__file__).resolve().parent.parent
BASEPAL=0x296300
COMBO_ANIMS={
 1:[0x25F7C8,0x25F23C,0x25DB0A],
 2:[0x25C240,0x25C762],
 3:[0x25C762,0x25EC78],
 4:[0x25B3D8,0x25DD1C],
 5:[0x25F792,0x25F30E],
 6:[0x25F76A,0x25E072,0x25BACE,0x25ECC6],
 7:[0x25CD36],
 8:[0x25B62E,0x25B0C8,0x25DFB4,0x25BF14],
 9:[],
 10:[0x25ED8C,0x25ED7C,0x25ED84,0x25ED94],
 11:[0x25DD1C,0x25CCC0],
 12:[0x25E226,0x25B62E],
 13:[0x25FDDE,0x25FDCC,0x25FDF0,0x25F1FE],
 14:[0x25EE8C],
}
NAMES=['','Paradinblw','Firestorm','Blizzard','Conduct Thunder','Grand Cross','Silent Wave','Shooting Star','Triblaster','Holocaust','Black Hole','Circuit Breaker','Purifying Light','Lethal Image','Destruct']

def palette(rom,cid):
 w=[int.from_bytes(rom.read(BASEPAL+i*2,2),'big') for i in range(16)]
 p=0xAC40+(cid-1)*8
 w[10:14]=[int.from_bytes(rom.read(p+i,2),'big') for i in range(0,8,2)]
 return [md2gba(x) for x in w]

def combo_frames(rom,cid,tab):
 chunks,ptrs,counts=plc_chunks(rom,0x16+cid,loader=2)
 pool=b''.join(chunks);base=0x9B80;loader_index=0x16+cid
 if cid==7:
  chunks,ptrs,counts=plc_chunks(rom,0x2F,loader=2);pool=b''.join(chunks);loader_index=0x2F
 if cid==11 and tab==0x25DD1C:
  chunks,ptrs,counts=plc_chunks(rom,0x30,loader=2);pool=b''.join(chunks);base=0xA540;loader_index=0x30
 if tab in PHYSICAL or tab in SLASHER:
  n=int.from_bytes(rom.read(tab,2),'big');maps=[];holds=[];arts=[];addresses=[]
  for i in range(n):
   e=rom.read(tab+2+i*4,4);fp=tab+4+int.from_bytes(e[2:],'big',signed=True);holds.append(e[1]);addresses.append(fp)
   if tab in PHYSICAL:
    artptr=int.from_bytes(rom.read(fp,4),'big');m=mapping(rom,fp+4)
    high=max((p&0x7FF)+w*h for x,y,w,h,p in records(m));art=rom.read(artptr,high*32)
   else:
    m=mapping(rom,fp);art=pool[DESTINATIONS[cid][tab]-base:]
   maps.append(m);arts.append(art)
  return maps,holds,arts,{'encoding':'physical' if tab in PHYSICAL else 'slasher','frame_records':addresses,'loader':2,'plc':loader_index,'plc_sources':ptrs}
 maps,holds=anim_table(rom,tab);dest=DESTINATIONS[cid][tab];art=pool[dest-base:]
 return maps,holds,[art]*len(maps),{'encoding':'standard','vram_base':base,'vram_destination':dest,'loader':2,'plc':loader_index,'plc_sources':ptrs}

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--rom',required=True,type=Path);a=ap.parse_args();raw=a.rom.read_bytes()
 if len(raw)!=0x300000: raise SystemExit('expected 3 MiB PSIV source ROM')
 rom=Rom(raw);bank=bytearray();parts=[];anims=[];descs=[(0,0,0,0)]*15;manifest={'combos':{}}
 shared_banks=[(ROOT/'gba/source_ability_effects.bin').read_bytes(),(ROOT/'gba/source_enemy_effects.bin').read_bytes()]
 aliases={}
 cache={}
 def put(b,align=4):
  k=(align,bytes(b))
  if k in cache:return cache[k]
  while len(bank)%align:bank.append(0)
  o=len(bank);bank.extend(b);cache[k]=o;return o
 def put_part(dat):
  if dat not in aliases:
   result=None
   for bi,blob in enumerate(shared_banks,1):
    at=blob.find(dat)
    while at>=0 and at%4:at=blob.find(dat,at+1)
    if at>=0:result=(at,bi);break
   aliases[dat]=result
  return aliases[dat] or (put(dat),0)
 for cid in range(1,15):
  chunks,_,_=plc_chunks(rom,0x16+cid,loader=2);ao=sum(map(len,chunks))
  first=len(anims);used=[];skipped=[];sources=[]
  for tab in COMBO_ANIMS[cid]:
   maps,holds,frame_art,source=combo_frames(rom,cid,tab)
   vis=[m for m in maps if m is not None]
   if not vis: skipped.append([hex(tab),'empty']);continue
   for m,art in zip(maps,frame_art):
    if m:assert max((p&0x7ff)+w*h for x,y,w,h,p in records(m))<=len(art)//32,(cid,hex(tab),'unloaded source tiles')
   bs=[bounds(m) for m in vis];u=(min(x[0] for x in bs),min(x[1] for x in bs),max(x[2] for x in bs),max(x[3] for x in bs));regs=split(u)
   live=sum(legal(r[2]-r[0],r[3]-r[1])[0]*legal(r[2]-r[0],r[3]-r[1])[1]//2 for r in regs)
   if live>0x2000: skipped.append([hex(tab),'live OBJ >8KiB']);continue
   fp=len(parts)
   try:
    for reg in regs:
     cw,ch=legal(reg[2]-reg[0],reg[3]-reg[1]);frames=[]
     for m,art in zip(maps,frame_art):
      if m is None: frames.append(bytes(cw*ch//2))
      else:
       cx,cy=(reg[0]+reg[2])//2,(reg[1]+reg[3])//2
       ox,oy=cx-cw//2,cy-ch//2
       pix,sw,sh=render_indices(bytes(art),m,(ox,oy,ox+cw,oy+ch));frames.append(pack_canvas(pix,cw,ch,cw,ch))
     dat=b''.join(frames);off,shared=put_part(dat);cx,cy=(reg[0]+reg[2])//2,(reg[1]+reg[3])//2;geom,ox,oy=obj_part_meta(cw,ch,cx,cy);parts.append((off,cw*ch//2,geom,shared,ox,oy))
   except Exception as e:
    del parts[fp:];skipped.append([hex(tab),f'render {e}']);continue
   ho=put(bytes(max(1,min(255,int(h))) for h in holds));anims.append((fp,len(regs),len(maps),ho,tab));used.append(tab);sources.append(dict(table=f'{tab:06X}',**source))
  # kind 1: source sprite controller lowered; kind 2: source screen/palette controller.
  kind=1 if len(anims)>first else 2
  descs[cid]=(first,len(anims)-first,kind,cid)
  manifest['combos'][str(cid)]={'name':NAMES[cid],'plc_index':0x16+cid,'plc_loader':2,'art_bytes':ao,'sources':sources,'tables_used':[hex(x) for x in used],'tables_skipped':skipped,'kind':kind}
 out=ROOT/'gba/source_combo_effects.bin';out.write_bytes(bank)
 h='''#pragma once\n#include <stdint.h>\ntypedef struct{uint32_t off;uint16_t frame_bytes;uint8_t geom,reserved;int8_t ox,oy;uint16_t pad;} Ps4SourceComboPart;\ntypedef struct{uint16_t first_part;uint8_t part_count,frame_count;uint32_t holds_off;uint32_t source_table;} Ps4SourceComboAnim;\ntypedef struct{uint16_t first_anim;uint8_t anim_count,kind,palette_id;} Ps4SourceComboDesc;\nextern const uint8_t ps4_source_combo_bank[];\nextern const Ps4SourceComboPart ps4_source_combo_parts[];\nextern const Ps4SourceComboAnim ps4_source_combo_anims[];\nextern const uint16_t ps4_source_combo_palettes[15][16] __attribute__((aligned(4)));\nextern const Ps4SourceComboDesc ps4_source_combo[15];\n'''
 (ROOT/'gba/source_combo_effects.h').write_text(h+f'#define PS4_SOURCE_COMBO_ANIM_COUNT {len(anims)}u\n')
 c=['#include "source_combo_effects.h"','const Ps4SourceComboPart ps4_source_combo_parts[]={']
 for p in parts:c.append('{%uu,%uu,%uu,%uu,%d,%d,0u},'%p)
 c+=['};','const Ps4SourceComboAnim ps4_source_combo_anims[]={']
 for q in anims:c.append('{%uu,%uu,%uu,%uu,0x%06Xu},'%q)
 c+=['};','const uint16_t ps4_source_combo_palettes[15][16] __attribute__((aligned(4)))={','{0},']
 for cid in range(1,15):c.append('{'+','.join('0x%04Xu'%v for v in palette(rom,cid))+'},')
 c+=['};','const Ps4SourceComboDesc ps4_source_combo[15]={']
 for d in descs:c.append('{%uu,%uu,%uu,%uu},'%d)
 c+=['};',''];(ROOT/'gba/source_combo_effects.c').write_text('\n'.join(c))
 (ROOT/'gba/source_combo_effects_data.s').write_text('.section .rodata.source_combo,"a",%progbits\n.balign 4\n.global ps4_source_combo_bank\nps4_source_combo_bank:\n.incbin "gba/source_combo_effects.bin"\n.balign 4\n')
 (ROOT/'source_combo_effects_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('source combo bank',len(bank),'bytes parts',len(parts),'anims',len(anims))
 for cid in range(1,15):print(cid,NAMES[cid],manifest['combos'][str(cid)]['tables_used'],'kind',manifest['combos'][str(cid)]['kind'])
if __name__=='__main__':main()
