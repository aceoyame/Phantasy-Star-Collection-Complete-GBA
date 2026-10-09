#!/usr/bin/env python3
"""Independent source-mapping pixel check for all appended R41 OBJ carriers.
Each output pixel is sampled by finding its original mapping piece; the GBA
packed/tiled bank is decoded separately. The complete R40 bank is retained.
"""
from pathlib import Path
import json
from PIL import Image,ImageDraw
from md_compression import nemesis_decompress
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';rom=(R/'references/roms/ps4built.bin').read_bytes();bank=(R/'gba/r31_field_poses.bin').read_bytes();j=json.loads((R/'generated/r41_field_poses_report.json').read_text())
u32=lambda p:int.from_bytes(rom[p:p+4],'big')
arts={p:nemesis_decompress(rom[p:]) for p in [0x1B1D06,0x1D712C,0x1A5F38]}
sheet=Image.new('RGB',(8*100,10*100),(18,22,30));draw=ImageDraw.Draw(sheet);pixels=0
for index,p in enumerate(j['poses']):
 t,face,frame=p['type'],p['facing'],p['frame'];table=0x4A504 if t==0x8194 else (0x502C2 if t==0x338 else 0x502F6);srcface=face-32 if t==0x8194 else face;seq=u32(table+srcface);n=rom[seq]&127;start=seq+(2 if not rom[seq]&128 else 1+n+((1+n)&1));mapping=u32(start+frame*4)
 assert frame<n and mapping==p['source_mapping']
 art=arts[0x1A5F38 if t==0x8194 else (0x1D712C if t==0x338 else 0x1B1D06)];pieces=[]
 for k in range(rom[mapping]+1):
  q=mapping+2+k*6;attr=int.from_bytes(rom[q+2:q+4],'big');x=int.from_bytes(rom[q+4:q+5],'big',signed=True);y=int.from_bytes(rom[q:q+1],'big',signed=True);w=8*(((rom[q+1]>>2)&3)+1);h=8*((rom[q+1]&3)+1);pieces.append((x,y,w,h,attr))
 im=Image.new('RGB',(p['w'],p['h']))
 for oy in range(p['h']):
  for ox in range(p['w']):
   source_x=ox+p['x'];source_y=oy+p['y'];expected=0
   for x,y,w,h,attr in pieces:
    if not (x<=source_x<x+w and y<=source_y<y+h):continue
    sx=source_x-x;sy=source_y-y
    if attr&0x800:sx=w-1-sx
    if attr&0x1000:sy=h-1-sy
    tile=(attr&0x7FF)+(sx//8)*(h//8)+sy//8;offset=tile*32+(sy%8)*4+(sx%8)//2;assert offset<len(art)
    color=(art[offset]>>(0 if sx&1 else 4))&15
    if color:expected=color
   offset=p['offset']+((oy//8)*(p['w']//8)+ox//8)*32+(oy%8)*4+(ox%8)//2
   actual=(bank[offset]>>(4 if ox&1 else 0))&15;assert actual==expected,(t,face,frame,ox,oy,actual,expected)
   im.putpixel((ox,oy),(expected*17,)*3);pixels+=1
 x=(index%8)*100;y=(index//8)*100;sheet.paste(im,(x+8,y+22));draw.text((x+4,y+3),f'{t:03X}/{face:X}/{frame}',fill='white')
sheet.save(O/'R41_ACTOR_PREVIEW.png');(O/'R41_POSE_PROBE.json').write_text(json.dumps({'scope':__doc__,'new_poses':len(j['poses']),'new_bank_bytes':j['appended_bytes'],'retained_r40_bytes':j['retained_r40_bytes'],'pixels_compared':pixels,'all_pixels_match_original_art':True,'preview':'grayscale source indices, not a final palette/display screenshot'},indent=2)+'\n');print('PASS',len(j['poses']),'new OBJ poses;',pixels,'pixels match independent original mappings')
