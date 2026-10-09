#!/usr/bin/env python3
"""Lower all source melee body variants, trails and android gun packages."""
from pathlib import Path
import json,re
from generate_enemy_animation_frames import Rom
from generate_enemy_attack_frames import mapping,records,bounds,split,legal
from generate_source_ability_effects import render_indices,pack_canvas,obj_part_meta
from generate_technique_objects import anim_table,plc_chunks
R=Path(__file__).resolve().parents[1];raw=(R/'references/roms/ps4built.bin').read_bytes();rom=Rom(raw)
def u16(p):return int.from_bytes(raw[p:p+2],'big')
def u32(p):return int.from_bytes(raw[p:p+4],'big')&0xffffff
MELEE={0:0x8BAA,2:0x8C74,3:0x8C8A,4:0x8CAA,5:0x8CCA,8:0x8CE2,10:0x8D02}
THRESHOLD={0:3,2:3,4:5,5:5,10:3}
shared=[(R/'gba'/f'source_{s}_effects.bin').read_bytes() for s in ['ability','enemy','combo']]
bank=bytearray();parts=[];anims=[];manifest=[];cache={};aliases={}
def put(b):
 b=bytes(b)
 if b in cache:return cache[b]
 while len(bank)%4:bank.append(0)
 at=len(bank);bank.extend(b);cache[b]=at;return at
def payload(b):
 b=bytes(b)
 if b not in aliases:
  q=None
  for tag,blob in enumerate(shared,1):
   at=blob.find(b)
   while at>=0 and at%4:at=blob.find(b,at+1)
   if at>=0:q=(at,tag);break
  aliases[b]=q
 return aliases[b] or (put(b),0)
def flip(m):
 if m is None:return None
 b=bytearray(m)
 for i in range(b[0]+1):
  p=1+i*6;b[p+4]=b[p+5];v=((b[p+2]<<8)|b[p+3])^0x800;b[p+2]=v>>8;b[p+3]=v&255
 return bytes(b)
def emit(table,maps,holds,arts,info):
 bs=[bounds(m) for m in maps if m];union=(min(b[0] for b in bs),min(b[1] for b in bs),max(b[2] for b in bs),max(b[3] for b in bs))
 first=len(parts);live=0
 for reg in split(union):
  w,h=legal(reg[2]-reg[0],reg[3]-reg[1]);live+=w*h//2
  g,ox,oy=obj_part_meta(w,h,(reg[0]+reg[2])//2,(reg[1]+reg[3])//2);frames=[]
  for m,art in zip(maps,arts):
   if not m:frames.append(bytes(w*h//2));continue
   pix,sw,sh=render_indices(art,m,(ox,oy,ox+w,oy+h));frames.append(pack_canvas(pix,w,h,w,h))
  # Frame-level sharing keeps duplicate body poses in one ROM span while
  # retaining every original frame and hold. High four bits select the bank.
  offsets=[]
  for frame in frames:
   off,tag=payload(frame);offsets.append((tag<<28)|off)
  off=put(b''.join(v.to_bytes(4,'little') for v in offsets));parts.append((off,w*h//2,g,4,ox,oy,1 if info['kind'] in ('melee','slash_body') else 0))
 assert live<=0x2000 and len(parts)-first<=48,(info,live)
 i=len(anims);anims.append((first,len(parts)-first,len(maps),put(bytes(max(1,h) for h in holds)),table))
 manifest.append(dict(index=i,table=f'{table:06X}',holds=holds,live_bytes=live,**info));return i
melee=[[[0xffff,0xffff] for v in range(3)] for i in range(11)];mcache={}
for actor,at in MELEE.items():
 for variant in range(3):
  table=u32(at+variant*4);dual=variant==2 and actor in THRESHOLD
  for left in range(2):
   key=(table,dual,left)
   if key not in mcache:
    n=u16(table);maps=[];arts=[];holds=[];fps=[];ap=[];flips=[]
    for f in range(n):
     fp=table+4+int.from_bytes(raw[table+4+4*f:table+6+4*f],'big',signed=True);artp=u32(fp);m=mapping(rom,fp+4)
     high=max((p&0x7ff)+w*h for x,y,w,h,p in records(m));art=raw[artp:artp+high*32]
     turned=bool(left or (dual and f>=THRESHOLD[actor]));maps.append(flip(m) if turned else m);arts.append(art);holds.append(raw[table+3+4*f]);fps.append(fp+4);ap.append(artp);flips.append(turned)
    mcache[key]=emit(table,maps,holds,arts,dict(kind='melee',actor=actor,variant=variant,left=left,frame_maps=fps,raw_art=ap,flipped=flips))
   melee[actor][variant][left]=mcache[key]
# BattleObj_Crosscut/Rayblade create a separate close-range body at $108.
slash_bodies=[]
for skill,table in [(1,0x25F792),(2,0x25F7BA)]:
 maps=[];arts=[];holds=[];fps=[];ap=[]
 for f in range(u16(table)):
  fp=table+4+int.from_bytes(raw[table+4+4*f:table+6+4*f],'big',signed=True);artp=u32(fp);m=mapping(rom,fp+4)
  high=max((p&0x7ff)+w*h for x,y,w,h,p in records(m));maps.append(m);arts.append(raw[artp:artp+high*32]);holds.append(raw[table+3+4*f]);fps.append(fp+4);ap.append(artp)
 slash_bodies.append(emit(table,maps,holds,arts,dict(kind='slash_body',skill=skill,frame_maps=fps,raw_art=ap)))
items={}
for item in range(1,0x90):
 wid=raw[0x883A+item];plc=raw[0x8B0A+item]
 if wid and plc:items.setdefault(wid,(item,plc))
trails=[[0xffff,0xffff] for i in range(0x1d)];guns=[[0xffff]*3 for i in range(0x90)]
for wid in range(1,0x16):
 item,pi=items[wid];table=u32(0x36FF2+wid*4);maps,holds=anim_table(rom,table);chunks,_,_=plc_chunks(rom,pi);art=(bytes(6*32) if pi==0x77 else b'')+b''.join(chunks)
 for left in range(2):trails[wid][left]=emit(table,[flip(m) if left else m for m in maps],holds,[art]*len(maps),dict(kind='trail',weapon=wid,item=item,plc=pi,left=left,base=0x9B80,destination=0x9B80))
launch=[0x25BA40,0x25BDC8,0x25BA40,0x25BA40,0x25B62E,0x25B62E,0x25BDC8]
hit=[(0x25AF70,0x9DA0),(0x25BB94,0x9B80),(0x25AF70,0x9DA0),(0x25BE8E,0x9DA0),(0x25BF14,0x9F40),(0x25CD36,0x9F40),(0x25BB94,0x9B80)]
for item in range(1,0x90):
 wid=raw[0x883A+item];pi=raw[0x8B0A+item]
 if not 0x16<=wid<=0x1C:continue
 chunks,_,_=plc_chunks(rom,pi);art=b''.join(chunks)
 for phase,(table,dest) in enumerate([(launch[wid-0x16],0x9B80),hit[wid-0x16]]+([(0x25B242,0xA0A0)] if wid==0x18 else [])):
  maps,holds=anim_table(rom,table);piece=art[dest-0x9B80:];guns[item][phase]=emit(table,maps,holds,[piece]*len(maps),dict(kind='gun',weapon=wid,item=item,plc=pi,phase=phase,base=0x9B80,destination=dest))
(R/'gba/r46_weapon_effects.bin').write_bytes(bank)
h='''#pragma once
#include "source_ability_effects.h"
extern const uint8_t r46_weapon_bank[];
extern const Ps4SourceAbilityPart r46_weapon_parts[];
extern const Ps4SourceAbilityAnim r46_weapon_anims[];
extern const uint16_t r46_melee_map[11][3][2];
extern const uint16_t r46_slash_body_map[2];
extern const uint16_t r46_weapon_trails[29][2];
extern const uint16_t r46_gun_map[144][3];
'''+f'#define R46_WEAPON_ANIM_COUNT {len(anims)}u\n'
(R/'gba/r46_weapon_effects.h').write_text(h)
c=['#include "r46_weapon_effects.h"','const Ps4SourceAbilityPart r46_weapon_parts[]={']
c+=['{%du,%du,%du,%du,%d,%d,%du},'%p for p in parts];c+=['};','const Ps4SourceAbilityAnim r46_weapon_anims[]={'];c+=['{%du,%du,%du,%du,0x%06Xu},'%a for a in anims];c+=['};','const uint16_t r46_melee_map[11][3][2]={']
c+=['{'+','.join('{%du,%du}'%tuple(v) for v in a)+'},' for a in melee];c+=['};','const uint16_t r46_slash_body_map[2]={'+','.join(str(i)+'u' for i in slash_bodies)+'};','const uint16_t r46_weapon_trails[29][2]={'];c+=['{%du,%du},'%tuple(a) for a in trails];c+=['};','const uint16_t r46_gun_map[144][3]={'];c+=['{%du,%du,%du},'%tuple(a) for a in guns];c+=['};',''];(R/'gba/r46_weapon_effects.c').write_text('\n'.join(c))
(R/'gba/r46_weapon_effects_data.s').write_text('.section .rodata.r46_weapons,"a",%progbits\n.balign 4\n.global r46_weapon_bank\nr46_weapon_bank:\n.incbin "gba/r46_weapon_effects.bin"\n.balign 4\n')
report={'bank_bytes':len(bank),'animation_records':len(anims),'parts':len(parts),'melee_actors':list(MELEE),'entries':manifest,'scope':'Original melee body/equipment variants and left-hand mapping coordinates; all 21 close-range trails and all 10 gun item packages. This asset lowering is separate from hit/miss/reaction/controller integration.'}
(R.parent/'build_artifacts/R46_WEAPON_ASSET_MANIFEST.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS weapon art',len(bank),'bytes',len(anims),'anims',len(parts),'parts')
