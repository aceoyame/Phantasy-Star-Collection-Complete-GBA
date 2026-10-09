#!/usr/bin/env python3
"""Fuse the verified shared eight-MOVE VRAM group; safe to rerun."""
from pathlib import Path
R=Path(__file__).resolve().parents[2];p=R/'source/additional/aot/generated/aot_code.c'
s=p.read_text()
if 'vram_moves:' in s:
 print('VRAM group already fused');raise SystemExit(0)
for game in ['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei']:
 data=(R/f'additional_roms/{game}_Adventure_-_English.bin').read_bytes()
 assert data[0x3218:0x3228]==bytes.fromhex('2a98'*8),game
start=s.index('B3218:');end=s.index('B3228:',start);old=s[start:end]
for j in range(8):
 assert f'B{0x3218+2*j:X}: if((s32)(cyc-lim)>=0){{ i={32+j}; goto ex; }}' in old
assert old.count('W32(ea1,mv);')==8 and old.count('NZL(16u,mv,')==8
block=''.join(f'B{0x3218+j*2:X}: i={32+j}u; goto vram_moves;\n' for j in range(8))
block+='''vram_moves:
 if((s32)(cyc-lim)>=0)goto ex;
 {
  u32 count=40u-i,room=lim-cyc,n=(room+19u)/20u;
  if(n>count)n=count;
  if((AA[5]&0xFFFFFEu)==0xC00000u&&md_native_vram_copy(AA[0],n)){
   AA[0]+=4u*n;cyc+=20u*n;i+=n;
  }else do{
   u32 source=AA[0];AA[0]+=4u;cyc+=12u;
   u32 value=R32(source);cyc+=8u;W32(AA[5],value);i++;
  }while(i<40u&&(s32)(cyc-lim)<0);
 }
 if(i<40u)goto ex;
 goto B3228;
'''
p.write_text(s[:start]+block+s[end:]);print('Fused shared VRAM group')
