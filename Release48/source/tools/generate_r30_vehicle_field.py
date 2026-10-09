#!/usr/bin/env python3
"""Convert retail vehicle field mappings to native OBJ poses, offline."""
from pathlib import Path
import re
from md_compression import nemesis_decompress
R=Path(__file__).resolve().parents[1];rom=(R/'references/roms/ps4built.bin').read_bytes();asm=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text(errors='replace')
def mapping(name):
 vals=[]
 for line in asm.split('\n'+name+':',1)[1].splitlines():
  q=line.split(';')[0].strip()
  if not q:continue
  m=re.match(r'dc\.b\s+(.*)',q)
  if not m:break
  vals.extend(int(v.strip().replace('$','0x'),0) for v in m[1].split(','))
 assert len(vals)==2+(vals[0]+1)*6
 return bytes(vals)
poses=[]
for vi,ad in enumerate([0x296320,0x296A54,0x2971A4]):
 art=nemesis_decompress(rom[ad:]);faces=[]
 for face in ['Down','Up','Right','Left']:
  frames=[]
  for moving in [False,True]:
   m=mapping('Mappings_Hydrofoil'+face if vi==2 else 'Mappings_Vehicle'+('Moving' if moving else 'Idle')+face);pix=bytearray(64*32)
   for j in range(m[0]+1):
    q=m[2+j*6:8+j*6];y=int.from_bytes(q[:1],'big',signed=True);x=int.from_bytes(q[4:5],'big',signed=True)+4;sz=q[1];wt=((sz>>2)&3)+1;ht=(sz&3)+1;pat=int.from_bytes(q[2:4],'big')
    for yy in range(ht*8):
     for xx in range(wt*8):
      sx=wt*8-1-xx if pat&0x800 else xx;sy=ht*8-1-yy if pat&0x1000 else yy;tile=(pat&0x7ff)+(sx>>3)*ht+(sy>>3);a=tile*32+(sy&7)*4+((sx&7)>>1);assert a<len(art)
      ci=(art[a]&15) if sx&1 else art[a]>>4
      if ci and 0<=x+xx<64 and 0<=y+yy<32:pix[(y+yy)*64+x+xx]=ci
   packed=bytearray()
   for ty in range(4):
    for tx in range(8):
     for y in range(8):
      for x in range(0,8,2):
       k=(ty*8+y)*64+tx*8+x;packed.append(pix[k]|(pix[k+1]<<4))
   frames.append(packed)
  faces.append(frames)
 poses.append(faces)
h=['/* Retail field vehicle art and mappings, offline native GBA 1D OBJ. */','static const uint8_t r30_vehicle_field[3][4][2][1024]={']
for vehicle in poses:
 h.append('{')
 for face in vehicle:h.append('{'+','.join('{'+','.join(str(x) for x in frame)+'}' for frame in face)+'},')
 h.append('},')
h.append('};');(R/'gba/r30_vehicle_field.h').write_text('\n'.join(h)+'\n');print('R30 vehicle field poses:',24,'bytes:',24576)
