#!/usr/bin/env python3
"""Decode only the three source PD backgrounds; accepted banks stay untouched."""
from pathlib import Path
import json,struct
from md_compression import nemesis_decompress,kosinski_decompress
R=Path(__file__).resolve().parents[1];rom=(R/'references/roms/ps4built.bin').read_bytes()
bank=bytearray();rows=[];phase_rows=[];pool=[];row_ids={}
for art,mp,kos in [(0x22EDD6,0x22F82C,True),(0x22B392,0x22BF98,False),(0x2274B2,0x228334,False)]:
 pixels=nemesis_decompress(rom[art:]);mapping=kosinski_decompress(rom[mp:]) if kos else rom[mp:mp+3072]
 assert len(mapping)==3072 and len(pixels)<=8192
 # Outside the 192-line source map, the renderer uses tile zero.  Its
 # source pixels are blank in all three backgrounds, allowing a DMA fill.
 assert pixels[:32]==bytes(32)
 ao=len(bank);bank+=pixels;mo=len(bank)
 bank+=struct.pack('<1536H',*struct.unpack('>1536H',mapping))
 words=struct.unpack('>1536H',mapping);texture=bytearray()
 assert all(((e>>13)&3)<=1 for e in words)
 for y in range(192):
  for x in range(512):
   e=words[(y//8)*64+x//8];tx=(x&7)^(7 if e&0x800 else 0);ty=(y&7)^(7 if e&0x1000 else 0);o=(e&0x7FF)*32+ty*4+tx//2
   ci=((pixels[o]>>(0 if tx&1 else 4))&15) if o<len(pixels) else 0
   texture.append((192 if ci and e&0x2000 else 208)+ci)
 ids=[]
 for phase in range(4):
  for y in range(192):
   row=bytes(texture[y*512+((x*4//3-phase)&511)] for x in range(384))
   if row not in row_ids:row_ids[row]=len(pool);pool.append(row)
   ids.append(row_ids[row])
 po=len(bank);bank+=struct.pack('<768H',*ids);phase_rows.append(ids)
 rows.append((ao,len(pixels),mo,po))
pool_off=len(bank);bank+=b''.join(pool)
(R/'gba/r45_pd_art.bin').write_bytes(bank)
(R/'gba/r45_pd_art.h').write_text('/* Original PSIV Nemesis/Kosinski/raw data; exact four-phase projected scroll rows.\n * Metadata: art offset/bytes, source map offset, phase-row-index offset. */\nextern const uint8_t r45_pd_art_bank[];\n#define R46_PD_ROWS_OFF %du\nstatic const uint32_t r45_pd_bg_meta[3][4]={\n'%pool_off+''.join(' {%du,%du,%du,%du},\n'%r for r in rows)+'};\n')
(R.parent/'build_artifacts/R45_PD_ART_SOURCE.json').write_text(json.dumps({'bytes':len(bank),'phase_rows':len(pool),'phase_row_bytes':384,'phase_pool_offset':pool_off,'backgrounds':[{'rom_art':hex(a),'rom_map':hex(m),'decoded_art_bytes':r[1],'map_words':1536} for (a,m),r in zip([(0x22EDD6,0x22F82C),(0x22B392,0x22BF98),(0x2274B2,0x228334)],rows)],'accepted_audio_pose_banks_regenerated':False},indent=2)+'\n')
print('PASS appended original PD backgrounds',len(bank),'bytes')
