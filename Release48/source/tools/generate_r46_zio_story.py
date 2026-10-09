#!/usr/bin/env python3
"""Scoped Zio3 banks: original root VRAM pool, stage PLCs, and Plane B.
Enemy $10 addresses the FIRST Nemesis pointer, not the animated Plane-A pool.
No existing enemy/party/audio bank is regenerated.
"""
from pathlib import Path
import json,struct
from generate_enemy_animation_frames import Rom,enemy_record
from generate_enemy_attack_frames import mapping,records,bounds,split,legal
from generate_source_enemy_effects import pack_canvas,obj_part_meta
from generate_technique_objects import plc_chunks
from md_compression import nemesis_decompress,enigma_decompress
R=Path(__file__).resolve().parents[1]
rom=Rom((R/'references/roms/ps4built.bin').read_bytes())
root=nemesis_decompress(rom.tail(enemy_record(rom,0x98)[0]))
plcs={i:b''.join(plc_chunks(rom,i)[0]) for i in (0xE8,0xE9,0xEA)}
def table(p,uniform=False):
 if uniform:
  hold,n=rom.read(p,2);return [int.from_bytes(rom.read(p+2+i*4,4),'big') for i in range(n)],[hold]*n
 n=rom.read(p,1)[0];holds=list(rom.read(p+1,n));start=p+1+n;start+=(start&1)
 return [int.from_bytes(rom.read(start+i*4,4),'big') for i in range(n)],holds
spec=[]
for p,art,uniform in [(0x3425C,root,False),(0x34280,plcs[0xE8],True),(0x342B2,plcs[0xE8],True),(0x3409E,root,True)]:
 f,h=table(p,uniform);spec.append((p,art,f,h,2))
spec.append((0x32C54,root,[0x32C54,0x32C92],[48,4],2))
f,h=table(0x33F7A);spec.append((0x33F7A,plcs[0xE9],f,h,2))
spec.append((0x33160,plcs[0xEA],[0x33160,0x3316E,0x3317C,0x331DE],[4,10,1,1],1))
def render_priority(art,m,reg,palette):
 # VDP link order gives the FIRST opaque source sprite priority. Render all
 # records, then retain this palette's visible pixels so partitioning across
 # GBA palette banks cannot reverse Black Wave ball/wave overlap.
 x0,y0,x1,y1=reg;w,h=x1-x0,y1-y0;pix=bytearray(w*h);owner=bytearray([255])*(w*h)
 for x,y,tw,th,p in records(m):
  for dy in range(th*8):
   yy=y+dy
   if not y0<=yy<y1:continue
   for dx in range(tw*8):
    xx=x+dx
    if not x0<=xx<x1:continue
    at=(yy-y0)*w+xx-x0
    if owner[at]!=255:continue
    sx=tw*8-1-dx if p&0x800 else dx;sy=th*8-1-dy if p&0x1000 else dy
    ti=(p&2047)+sx//8*th+sy//8;assert ti<len(art)//32
    q=art[ti*32+(sy&7)*4+(sx&7)//2];ci=q>>(0 if sx&1 else 4)&15
    if ci:owner[at]=p>>13&3;pix[at]=ci if owner[at]==palette else 0
 return pix
bank=bytearray();parts=[];anims=[];evidence=[]
def put(b):
 while len(bank)%4:bank.append(0)
 off=len(bank);bank.extend(b);return off
for source,art,frames,holds,palbase in spec:
 ms=[mapping(rom,p) for p in frames];first=len(parts);palettes=sorted(set((p>>13)&3 for m in ms for x,y,w,h,p in records(m)))
 for pal in palettes:
  selected=[]
  for m in ms:
   rows=[m[1+i*6:7+i*6] for i in range(m[0]+1) if (int.from_bytes(m[3+i*6:5+i*6],'big')>>13)&3==pal]
   selected.append(bytes([len(rows)-1])+b''.join(rows) if rows else None)
  bs=[bounds(m) for m in selected if m];u=(min(b[0] for b in bs),min(b[1] for b in bs),max(b[2] for b in bs),max(b[3] for b in bs))
  for reg in split(u):
   sw,sh=reg[2]-reg[0],reg[3]-reg[1];cw,ch=legal(sw,sh);cx,cy=(reg[0]+reg[2])//2,(reg[1]+reg[3])//2;geom,ox,oy=obj_part_meta(cw,ch,cx,cy)
   payload=b''.join(pack_canvas(render_priority(art,m,(ox,oy,ox+cw,oy+ch),pal),cw,ch,cw,ch) for m in ms)
   parts.append((put(payload),cw*ch//2,geom,(palbase+pal)&3,ox,oy))
 anims.append((first,len(parts)-first,len(frames),put(bytes(holds)),source))
 evidence.append({'table':hex(source),'frames':[hex(p) for p in frames],'holds':holds,'art_bytes':len(art),'parts':len(parts)-first,'live_bytes':sum(p[1] for p in parts[first:]),'palette_lines':[(palbase+p)&3 for p in palettes]})
# EnemyInit_Zio's private Plane B, including the full 28 source rows.
art=nemesis_decompress(rom.tail(0x21D656));cells=enigma_decompress(rom.tail(0x21E656));bg=bytearray()
for y in range(160):
 sy=y*7//5
 for x in range(240):
  sx=x*4//3;e=cells[(sy//8)*64+sx//8];assert e>>13&3==0
  px=(sx&7)^(7 if e&0x800 else 0);py=(sy&7)^(7 if e&0x1000 else 0);q=art[(e&2047)*32+py*4+px//2]
  bg.append(240+((q>>(0 if px&1 else 4))&15))
bgoff=put(bg)
(R/'gba/r46_zio_story.bin').write_bytes(bank)
header='#pragma once\n#include "source_enemy_effects.h"\nextern const uint8_t r46_zio_bank[];\n'
header+='static const Ps4SourceEnemyPart r46_zio_parts[]={\n'+',\n'.join('{%du,%du,%du,%du,%d,%d,0u}'%p for p in parts)+'\n};\n'
header+='static const Ps4SourceEnemyAnim r46_zio_anims[]={\n'+',\n'.join('{%du,%du,%du,%du,0x%Xu}'%a for a in anims)+'\n};\n'
header+=f'#define R46_ZIO_BG_OFF {bgoff}u\n'
(R/'gba/r46_zio_story_data.h').write_text(header)
(R/'gba/r46_zio_story_data.s').write_text('.section .rodata.r46_zio,"a",%progbits\n.balign 4\n.global r46_zio_bank\nr46_zio_bank:\n.incbin "gba/r46_zio_story.bin"\n')
(R.parent/'build_artifacts/R46_CP15_ZIO_ASSETS.json').write_text(json.dumps({'bytes':len(bank),'animations':evidence,'background':'EnemyInit_Zio $21D656/$21E656, 240x160 direct source sampling'},indent=2)+'\n')
print('Zio source bank',len(bank),'bytes; live bytes', [e['live_bytes'] for e in evidence])
