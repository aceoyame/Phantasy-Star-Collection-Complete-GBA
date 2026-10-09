#!/usr/bin/env python3
"""Independent selected-bank, original-ROM tile/plane and coverage evidence."""
from pathlib import Path
import struct,re,json,collections
from md_compression import nemesis_decompress,enigma_decompress
R=Path(__file__).resolve().parents[1];G=R/'gba';REF=R/'references/claude_r46_cp05/gba';O=R.parent/'build_artifacts'
formats={'blocks':'<HBBBBBx','block_tiles':'<H','pieces':'<bbHBx','frames':'<HBx','tracks':'<BBBBBBHHhh2xIH2x','ents':'<HhhH','pals':'<16H','tickpal':'<HBxH','sfx':'<HBx','scene':'<HBBbbbxHH','bgev':'<HBbHHH','bgimg':'<II','bgpal':'<HHHHH','clips':'<BBBxHHH2xIHHHHHHHHH2x'}
def bank(directory,basename):
 b=(directory/basename).read_bytes();h=(directory/'r46_battle_clips.h').read_text();off={m[1]:int(m[2]) for m in re.finditer(r'^#define r46_(\w+) .*R46_TABLE_BASE\+(\d+)u',h,re.M)}
 def row(n,i):return struct.unpack_from(formats[n],b,off[n]+i*struct.calcsize(formats[n]))
 return b,off,row
old,oo,orow=bank(REF,'r46_clip_tables.bin');new,no,nrow=bank(G,'r46_recorded_clips.bin');opool=(REF/'r46_tile_pool.bin').read_bytes();npool=(G/'r46_recorded_tiles.bin').read_bytes();rom=(R/'references/roms/ps4built.bin').read_bytes()
charge=nemesis_decompress(rom[0x234306:]);maps={}
for i in range(4):
 q=rom[0x2d4a3+i*6:0x2d4a9+i*6];maps[q[4]-256 if q[4]>127 else q[4],q[0]-256 if q[0]>127 else q[0]]=(((q[1]>>2)&3)+1,(q[1]&3)+1,((q[2]<<8)|q[3])&2047)
def frame(row,pool,fi,fix=False):
 if fi==65535:return None
 st,n=row('frames',fi);res=[]
 for i in range(st,st+n):
  dx,dy,bl,flags=row('pieces',i);first,gw,gh,w,h,shape=row('blocks',bl);data=bytearray()
  for gy in range(gh):
   for gx in range(gw):
    ref=row('block_tiles',first+gy*gw+gx)[0];tile=bytes(32) if ref==65535 else pool[ref*32:ref*32+32]
    if fix:
     mw,mh,ti=maps[dx,dy];assert (mw,mh)==(w,h)
     md=charge[(ti+gx*h+gy)*32:(ti+gx*h+gy+1)*32] if gx<w and gy<h else bytes(32)
     tile=bytes((v>>4)|((v&15)<<4) for v in md)
    data+=tile
  res.append((dx,dy,flags,gw,gh,w,h,shape,bytes(data)))
 return tuple(res)
def rle(b,off,row,i):
 at,n=row('bgimg',i);raw=b[off['bg_rle']+at:off['bg_rle']+at+n];out=bytearray();p=0
 while p<len(raw):
  c=raw[p];p+=1
  if c&128:out.extend([raw[p]]*(c-128+3));p+=1
  else:out.extend(raw[p:p+c+1]);p+=c+1
 assert len(out)==38400 and p==len(raw);return bytes(out)
def canon(b,off,row,pool,c,fix):
 tracks=[]
 for i in range(c[4],c[4]+c[5]):
  t=row('tracks',i);es=[]
  for j in range(t[10],t[10]+t[11]):
   e=row('ents',j);es.append((frame(row,pool,e[0],fix and c[0]==1 and t[5]==56),)+e[1:])
  assert sum(e[3] for e in es)==t[7]-t[6];tracks.append((t[:10],tuple(es)))
 events=[]
 for name,st,n in [('tickpal',6,7),('sfx',8,9),('scene',10,11),('bgev',12,13),('bgpal',14,15)]:
  ev=[]
  for i in range(c[st],c[st]+c[n]):
   q=list(row(name,i))
   if name=='tickpal':q[2]=row('pals',q[2])
   if name=='scene' and q[2] in (3,4):q[7]=row('pals',q[7])
   if name=='bgpal':q[1:]=[row('pals',j) for j in q[1:]]
   if name=='bgev' and q[1]==1:
    q[3]=rle(b,off,row,q[3]);q[4]=struct.unpack_from('<160b',b,off['bg_shift']+q[4]*160) if q[4]!=65535 else None
   ev.append(tuple(q))
  events.append(tuple(ev))
 return c[:4],tuple(tracks),tuple(events)
oldclips=[orow('clips',i) for i in range((len(old)-oo['clips'])//36)];lookup={c[:3]:c for c in oldclips};peaks=[]
for i in range((len(new)-no['clips'])//36):
 c=nrow('clips',i);assert canon(new,no,nrow,npool,c,False)==canon(old,oo,orow,opool,lookup[c[:3]],True),('capture lowering mismatch',c[:3])
 states=[]
 for j in range(c[4],c[4]+c[5]):
  t=nrow('tracks',j);es=[]
  for k in range(t[10],t[10]+t[11]):
   e=nrow('ents',k);es.extend([e[0]]*e[3])
  states.append((t[6],t[7],es))
 pp=pt=0
 for tick in range(c[3]):
  pieces=0;blocks=set()
  for start,end,es in states:
   if start<=tick<end and es[tick-start]!=65535:
    fp,n=nrow('frames',es[tick-start]);pieces+=n;blocks.update(nrow('pieces',j)[2] for j in range(fp,fp+n))
  tiles=sum(nrow('blocks',j)[1]*nrow('blocks',j)[2] for j in blocks);pp=max(pp,pieces);pt=max(pt,tiles)
 assert c[5]<=64 and pp<=48 and pt<=256
 peaks.append({'key':c[:3],'ticks':c[3],'tracks':c[5],'pieces':pp,'live_tiles':pt})
# Independent original PLC table decode, then tile membership comparison.
tile_sources={}
item_ids=[0x4e,0x4f,0x50,0x51,0x62,0x71,0x8b,0x8e]
loaders=[(1,0xcd),(1,0xcf),(2,0x1f),(2,0x20)]+[(2,rom[0x2a8e28+(i-1)*22+7]-1) for i in item_ids]+[(2,i) for i in (0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2e)]
for loader,idx in loaders:
 base=0x27e88a if loader==1 else 0x27ea66;rel=int.from_bytes(rom[base+idx*2:base+idx*2+2],'big',signed=True);a=base+rel;n=int.from_bytes(rom[a:a+2],'big')+1
 for j in range(n):
  ptr=int.from_bytes(rom[a+2+j*4:a+6+j*4],'big')&0xffffff;art=nemesis_decompress(rom[ptr:])
  for k in range(0,len(art),32):tile_sources[art[k:k+32]]=ptr
for i in range(0,len(npool),32):
 md=bytes((v>>4)|((v&15)<<4) for v in npool[i:i+32]);assert md in tile_sources,('not original PLC tile',i//32)
# Black Hole's recorded plane pixels are compared directly with the original
# Nemesis 27D1B0 + Enigma 27D6DE, not the recorder's extraction algorithm.
plane=rle(new,no,nrow,0);art=nemesis_decompress(rom[0x27d1b0:]);mp=enigma_decompress(rom[0x27d6de:]);expected=bytearray()
for y in range(160):
 sy=y*7//5
 for x in range(240):
  sx=x*4//3;mi=(sy//8)*64+sx//8;pr=mp[mi] if mi<len(mp) else 0;xx=sx%8;yy=sy%8
  if pr&2048:xx=7-xx
  if pr&4096:yy=7-yy
  at=(pr&2047)*32+yy*4+xx//2;v=(art[at]>>(0 if xx&1 else 4))&15;expected.append(((pr>>13)&3)*16+v if v else 0)
assert plane==expected,'Black Hole original plane pixel mismatch'
budget_candidates=[]
for c in oldclips:
 states=[]
 for j in range(c[4],c[4]+c[5]):
  t=orow('tracks',j);es=[]
  for k in range(t[10],t[10]+t[11]):
   e=orow('ents',k);es.extend([e[0]]*e[3])
  assert len(es)==t[7]-t[6];states.append((t[6],t[7],es))
 pp=pt=0
 for tick in range(c[3]):
  n=0;blocks=set()
  for start,end,es in states:
   if start<=tick<end and es[tick-start]!=65535:
    fp,np=orow('frames',es[tick-start]);n+=np;blocks.update(orow('pieces',j)[2] for j in range(fp,fp+np))
  pp=max(pp,n);pt=max(pt,sum(orow('blocks',j)[1]*orow('blocks',j)[2] for j in blocks))
 if c[5]>64 or pp>48 or pt>512:budget_candidates.append({'key':c[:3],'tracks':c[5],'pieces_before_viewport_culling':pp,'live_tiles':pt})
source_pairs=set()
for enemy in range(153):
 a=0x2816bc+enemy*48
 source_pairs.update((enemy,v) for v in set(rom[a+36:a+44])|{v for v in rom[a+32:a+36] if v})
captured_pairs={c[1:3] for c in oldclips if c[0]==1}
report={'result':'PASS','captured_catalog':dict(collections.Counter(c[0] for c in oldclips)),'rom_enemy_pairs':len(source_pairs),'covered_rom_enemy_pairs':len(source_pairs&captured_pairs),'uncaptured_rom_enemy_pairs':sorted(source_pairs-captured_pairs),'extra_scripted_enemy_keys':sorted(captured_pairs-source_pairs),'pre_cull_budget_candidates':budget_candidates,'selected_peaks':peaks,'selected_original_tiles':len(npool)//32,'black_hole_original_plane_pixels':38400,'source_recording_limit':'CP05 supplies primitive raw clips but no source save states; actor anchors/random/status branches are not independently re-recorded. Pre-cull budget candidates require viewport-aware review; >64 track clipping is an explicit player limit.'}
(O/'R46_RECORDED_DATA_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS',len(peaks),'selected clips;',len(npool)//32,'original tiles; 38400 original plane pixels;',len(oldclips),'reference clips;',len(budget_candidates),'pre-cull capacity candidates')
