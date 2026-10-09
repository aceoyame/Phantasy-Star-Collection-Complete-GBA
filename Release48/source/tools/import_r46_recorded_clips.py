#!/usr/bin/env python3
"""Lower selected user-supplied Claude captures; leave PSC donor intact.

CP05 supplies primitive-only raw clip pickles, but no recorder save states.
The immutable supplied banks are retained as provenance. Fix shared DF3 art,
then retain captured holds, sprite offsets, palette and background events.
"""
from pathlib import Path
import struct,re,json
from md_compression import nemesis_decompress
R=Path(__file__).resolve().parents[1]
REF=R/'references/claude_r46_cp05/gba';G=R/'gba';O=R.parent/'build_artifacts'
raw=(REF/'r46_clip_tables.bin').read_bytes();pool=(REF/'r46_tile_pool.bin').read_bytes()
SPECS=[('blocks','R46Block',0,18488,'<HBBBBBx'),('block_tiles','uint16_t',18488,46416,'<H'),('pieces','R46Piece',46416,84188,'<bbHBx'),('frames','R46Frame',84188,90804,'<HBx'),('tracks','R46Track',90804,164244,'<BBBBBBHHhh2xIH2x'),('ents','R46Ent',164244,402540,'<HhhH'),('pals','R46Pal',402540,423884,'<16H'),('tickpal','R46TickPal',423884,442436,'<HBxH'),('sfx','R46Sfx',442436,453948,'<HBx'),('scene','R46Scene',453948,498024,'<HBBbbbxHH'),('bgev','R46BgEv',498024,499024,'<HBbHHH'),('bgimg','R46BgImg',499024,499032,'<II'),('bg_shift','int8_t',499032,504152,'<160b'),('bgpal','R46BgPal',504152,504564,'<HHHHH'),('bg_rle','uint8_t',504564,508548,'<B'),('clips','R46Clip',508548,525252,'<BBBxHHH2xIHHHHHHHHH2x')]
header=(REF/'r46_battle_clips.h').read_text()
source_offsets={m[1]:int(m[2]) for m in re.finditer(r'^#define r46_(\w+) .*R46_TABLE_BASE\+(\d+)u',header,re.M)}
SPECS=[(n,ty,source_offsets[n],source_offsets[SPECS[i+1][0]] if i+1<len(SPECS) else len(raw),fmt) for i,(n,ty,a,z,fmt) in enumerate(SPECS)]
src={n:[struct.unpack_from(fmt,raw,p) for p in range(a,z-struct.calcsize(fmt)+1,struct.calcsize(fmt))] for n,ty,a,z,fmt in SPECS}
ITEMS={0x4E,0x4F,0x50,0x51,0x62,0x71,0x8B,0x8E}
VEHICLES={1:(0,1,2),2:(0,5,7),3:(0,3,4)}
KEYS={(1,0x84,0x4B),(1,0x84,0x51),(4,9,0),(4,10,0)}|{(9,i,0) for i in ITEMS}|{(10,v,k) for v,ks in VEHICLES.items() for k in ks}
for i,c in enumerate(src['clips']):
 if c[:3] in KEYS:assert raw[source_offsets['clips']+i*36+3]==0,'Actor-bearing captures require separately verified rendering'
rom=(R/'references/roms/ps4built.bin').read_bytes()
charge=nemesis_decompress(rom[0x234306:]);assert len(charge)==14*32
mapping=rom[0x2D4A2:0x2D4A2+25];assert mapping[0]==3
mapped={}
for j in range(4):
 q=mapping[1+j*6:7+j*6];x=q[4]-256 if q[4]>127 else q[4];y=q[0]-256 if q[0]>127 else q[0]
 mapped[x,y]=(((q[1]>>2)&3)+1,(q[1]&3)+1,((q[2]<<8)|q[3])&2047)
expected={}
for c in src['clips']:
 if c[:3] not in {(1,0x84,0x4B),(1,0x84,0x51)}:continue
 for tr in src['tracks'][c[4]:c[4]+c[5]]:
  if tr[5]!=56:continue
  for e in src['ents'][tr[10]:tr[10]+tr[11]]:
   if e[0]==65535:continue
   f=src['frames'][e[0]];assert f[1]==4
   for piece in src['pieces'][f[0]:f[0]+f[1]]:
    w,h,tile=mapped[piece[0],piece[1]];bl=src['blocks'][piece[2]];assert (w,h)==(bl[3],bl[4])
    for gy in range(h):
     for gx in range(w):
      md=charge[(tile+gx*h+gy)*32:(tile+gx*h+gy+1)*32];gb=bytes((b>>4)|((b&15)<<4) for b in md)
      ref=src['block_tiles'][bl[0]+gy*bl[1]+gx][0]
      if ref==65535:
       assert not any(gb),('missing nonblank charge tile',tile,gx,gy)
       continue
      if ref in expected:assert expected[ref]==gb
      expected[ref]=gb
repaired=bytearray(pool);fixed_tiles=[]
for ref,gb in expected.items():
 if repaired[ref*32:ref*32+32]!=gb:fixed_tiles.append(ref);repaired[ref*32:ref*32+32]=gb
assert len(fixed_tiles)==6,fixed_tiles
pool=bytes(repaired)
dst={n:[] for n,*_ in SPECS};memo={n:{} for n,*_ in SPECS};newpool=bytearray();tiles={}
def tile_ref(old):
 if old==65535:return old
 if old not in tiles:tiles[old]=len(newpool)//32;newpool.extend(pool[old*32:old*32+32])
 return tiles[old]
def copy(n,i):
 if i in memo[n]:return memo[n][i]
 v=list(src[n][i]);ni=len(dst[n]);memo[n][i]=ni
 if n=='blocks':
  start=v[0];v[0]=len(dst['block_tiles'])
  dst['block_tiles'].extend((tile_ref(src['block_tiles'][j][0]),) for j in range(start,start+v[1]*v[2]))
 elif n=='frames':
  st=v[0];v[0]=len(dst['pieces'])
  for j in range(st,st+v[1]):
   q=list(src['pieces'][j]);q[2]=copy('blocks',q[2]);dst['pieces'].append(tuple(q))
 elif n=='tracks':
  st=v[10];v[10]=len(dst['ents'])
  for j in range(st,st+v[11]):
   q=list(src['ents'][j]);q[0]=copy('frames',q[0]) if q[0]!=65535 else 65535;dst['ents'].append(tuple(q))
 elif n=='bgimg':
  st,size=v;v[0]=len(dst['bg_rle']);dst['bg_rle'].extend(src['bg_rle'][st:st+size])
 elif n=='clips':
  for name,start,count in [('tracks',4,5),('tickpal',6,7),('sfx',8,9),('scene',10,11),('bgev',12,13),('bgpal',14,15)]:
   st=v[start];v[start]=len(dst[name])
   for j in range(st,st+v[count]):
    q=list(src[name][j])
    if name=='tracks':copy(name,j);continue
    if name=='tickpal':q[2]=copy('pals',q[2])
    elif name=='scene' and q[2] in (3,4):q[7]=copy('pals',q[7])
    elif name=='bgev' and q[1]==1:
     q[3]=copy('bgimg',q[3])
     if q[4]!=65535:q[4]=copy('bg_shift',q[4])
    elif name=='bgpal':q[1:]=[copy('pals',k) for k in q[1:]]
    dst[name].append(tuple(q))
 dst[n].append(tuple(v));return ni
for i,c in enumerate(src['clips']):
 if c[:3] in KEYS:copy('clips',i)
assert {q[:3] for q in dst['clips']}==KEYS
blob=bytearray();offsets={}
for n,ty,a,z,fmt in SPECS:
 while len(blob)%4:blob.append(0)
 offsets[n]=len(blob)
 for q in dst[n]:blob.extend(struct.pack(fmt,*q))
(G/'r46_recorded_clips.bin').write_bytes(blob);(G/'r46_recorded_tiles.bin').write_bytes(newpool)
h=(REF/'r46_battle_clips.h').read_text()
h=re.sub(r'#define R46_TABLE_BASE .*',r'extern const uint8_t r46_recorded_tables[];\n#define R46_TABLE_BASE ((uintptr_t)r46_recorded_tables)',h)
h=re.sub(r'#define R46_TABLE_BYTES .*',f'#define R46_TABLE_BYTES {len(blob)}u',h)
for n,ty,*_ in SPECS:h=re.sub(r'^#define r46_'+n+r' .*',f'#define r46_{n} ((const {ty}*)(R46_TABLE_BASE+{offsets[n]}u))',h,flags=re.M)
(G/'r46_battle_clips.h').write_text(h)
(G/'r46_recorded_clips.c').write_text('#include "r46_battle_clips.h"\nconst unsigned r46_clip_count='+str(len(KEYS))+'u;\n')
(G/'r46_recorded_clips_data.s').write_text('.section .rodata.r46_recorded,"a",%progbits\n.balign 4\n.global r46_recorded_tables\nr46_recorded_tables:\n.incbin "gba/r46_recorded_clips.bin"\n.balign 4\n.global r46_tile_pool\nr46_tile_pool:\n.incbin "gba/r46_recorded_tiles.bin"\n.balign 4\n')
report={'input':'User-supplied Claude R46_CP05.zip packed capture banks','raw_recordings_supplied':True,'recorder_save_states_supplied':False,'df3_charge_tiles_replaced_from_original_rom':len(fixed_tiles),'df3_charge_original_nemesis':'234306','df3_charge_original_mapping':'2D4A2','selected_keys':sorted(KEYS),'table_bytes':len(blob),'tile_bytes':len(newpool),'counts':{k:len(v) for k,v in dst.items()},'donor_modified':False,'scope':'Visual playback only; existing native callers execute gameplay once. Friendly item anchors, actor-bearing normal attacks and remaining party clips are not enabled.'}
(O/'R46_RECORDED_IMPORT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
