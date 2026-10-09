#!/usr/bin/env python3
"""Inspect primitive-only supplied raw clips and independently compare new tracks."""
from pathlib import Path
import collections,io,json,pickle,re,struct,tarfile
R=Path(__file__).resolve().parents[1];REF=R/'references/claude_r46_cp05';O=R.parent/'build_artifacts'
class PrimitiveOnly(pickle.Unpickler):
 def find_class(self,module,name):raise ValueError('Nonprimitive pickle global: '+module+'.'+name)
 def persistent_load(self,pid):raise ValueError('Persistent pickle reference')
with tarfile.open(REF/'tools_host/r46_recordings/clips_slim.tgz') as t:
 records={}
 for m in t.getmembers():
  if not m.isfile():continue
  assert m.size<20_000_000 and '..' not in Path(m.name).parts and not Path(m.name).is_absolute()
  records[Path(m.name).name]=PrimitiveOnly(io.BytesIO(t.extractfile(m).read())).load()
assert len(records)==569
raw=(R/'gba/r46_recorded_clips.bin').read_bytes();pool=(R/'gba/r46_recorded_tiles.bin').read_bytes();h=(R/'gba/r46_battle_clips.h').read_text()
off={m[1]:int(m[2]) for m in re.finditer(r'^#define r46_(\w+) .*R46_TABLE_BASE\+(\d+)u',h,re.M)}
fmt={'clips':'<BBBxHHH2xIHHHHHHHHH2x','tracks':'<BBBBBBHHhh2xIH2x','ents':'<HhhH','frames':'<HBx','pieces':'<bbHBx','blocks':'<HBBBBBx','block_tiles':'<H','pals':'<16H','tickpal':'<HBxH','sfx':'<HBx'}
def row(n,i):return struct.unpack_from(fmt[n],raw,off[n]+i*struct.calcsize(fmt[n]))
def packed_frame(fi):
 if fi==65535:return ()
 start,n=row('frames',fi);out=[]
 for i in range(start,start+n):
  dx,dy,bid,fl=row('pieces',i);at,gw,gh,w,hh,shape=row('blocks',bid)
  data=b''.join(bytes(32) if row('block_tiles',j)[0]==65535 else pool[row('block_tiles',j)[0]*32:row('block_tiles',j)[0]*32+32] for j in range(at,at+gw*gh))
  out.append((dx,dy,fl,gw,gh,w,hh,shape,data))
 return tuple(out)
checks=[]
for ci in range((len(raw)-off['clips'])//36):
 c=row('clips',ci)
 if c[0] not in (9,10):continue
 name='u_%02x.pkl'%c[1] if c[0]==9 else 'v_%d_%d.pkl'%(c[1],c[2]);d=records[name];r=d['clip'];assert c[3]==r['ticks']
 trs=sorted(r['tracks'],key=lambda q:(q['slot'],q['start']));assert len(trs)==c[5]
 for ti,tr in enumerate(trs):
  p=row('tracks',c[4]+ti);assert p[:10]==(*tr['A'],*tr['B'],tr['mode'],tr['slot'],tr['start'],tr['end'],tr['ax'],tr['bx'])
  expanded=[]
  for j in range(p[10],p[10]+p[11]):
   fi,x,y,run=row('ents',j);expanded.extend([(packed_frame(fi),x,y)]*run)
  want=[]
  for pieces,x,y in tr['ent']:
   ps=[]
   for dx,dy,bid,fl,line,prio in pieces:
    assert -128<=dx<=127 and -128<=dy<=127,'Visible-source piece must not be silently dropped'
    _,data,gw,gh,shape,w,hh=d['blocks'][bid];ps.append((dx,dy,fl|(line<<2)|(prio<<4),gw,gh,w,hh,shape[0]|(shape[1]<<2),data))
   want.append((tuple(ps),x,y))
  assert expanded==want,(name,ti)
 expected=[];last=[None]*4
 for tick,pals in enumerate(r['tick_pal']):
  for line,pal in enumerate(pals):
   if pal!=255 and r['palettes'][pal]!=last[line]:expected.append((tick,line,r['palettes'][pal]));last[line]=r['palettes'][pal]
 got=[row('tickpal',j) for j in range(c[6],c[6]+c[7])]
 assert [(tick,line,row('pals',pal)) for tick,line,pal in got]==expected,name
 assert [row('sfx',j) for j in range(c[8],c[8]+c[9])]==list(r['sfx']),name
 checks.append({'key':c[:3],'raw_file':name,'ticks':c[3],'tracks':c[5]})
skills={name:d['owner'] for name,d in records.items() if name.startswith('s_') and 'owner' in d};assert len(skills)==54
report={'result':'PASS','raw_recordings':len(records),'primitive_only_unpickler':True,'packed_reference_clips':557,'raw_attack_configurations':sum(n.startswith('a') for n in records),'raw_skill_owner_metadata':skills,'new_selected_raw_track_palette_sfx_matches':checks,'limits':'Raw clips are extracted source data, not original VDP snapshots or fresh branch re-recordings. Save states are missing. Normal attacks, friendly item anchors and all-caster/status/RNG coverage remain open.'}
(O/'R46_CLAUDE_CP05_REVIEW.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS 569 primitive raw recordings; 54 owner-tagged skills;',len(checks),'new selected raw schedules/frames/palettes/SFX match without dropped pieces')
