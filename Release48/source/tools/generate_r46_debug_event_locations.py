#!/usr/bin/env python3
"""Build the story-debug warp catalog from pinned source data, without new flags."""
from pathlib import Path
import sys,re,json,collections
S=Path(__file__).resolve().parents[1];sys.path.insert(0,str(S/'tools'));from md_compression import kosinski_decompress
rom=(S/'references/roms/ps4built.bin').read_bytes();src=(S/'gba/title_runtime_v06.c').read_text();hdr=(S/'gba/generated_simple_events_v0982.h').read_text()
word=lambda p:int.from_bytes(rom[p:p+2],'big')
maps={}
for line in (S/'gba/psc_map_desc.c').read_text().splitlines():
 if line.startswith('{'):
  vals=[int(v.rstrip('u'),0) for v in line.split('}')[0][1:].split(',')];id_=int(line.split('/* ')[1][:3],16)
  if vals[16]:maps[id_]=vals
inbound=collections.defaultdict(list)
for id_,v in maps.items():
 for start in (v[5],v[6]):
  for p in range(start,start+10240,10):
   if word(p)==65535:break
   dst=word(p+4)
   if dst in maps:inbound[dst].append((rom[p+6]*16,rom[p+7]*16,(rom[p+8]>>2)&3))

def default(id_):
 v=inbound.get(id_)
 return v[0] if v else (None,None,0)

def rows(name):
 text=hdr.split(' '+name+'[')[1].split('={')[1].split('};')[0]
 return [[int(v.rstrip('u'),0) for v in l.split('}')[0].lstrip()[1:].split(',')] for l in text.splitlines() if l.strip().startswith('{')]
conditions=collections.defaultdict(list)
for v in rows('ps4_simple_run_event_conditions'):conditions[v[0]].append((v[2],[],'bound flag trigger'))
for v in rows('ps4_run_event_flag_chain_conditions'):conditions[v[0]].append((v[1],[],'bound flag chain trigger'))
for v in rows('ps4_run_event_flag_coord_eq_conditions'):conditions[v[0]].append((v[1],[(ax,0,val) for ax,val in enumerate(v[-2:]) if val!=65535],'bound equal coordinate trigger'))
for v in rows('ps4_run_event_flag_coord_range_conditions'):
 conditions[v[0]].append((v[1],[tuple(v[13+n*3:16+n*3]) for n in range(v[12])],'bound coordinate range trigger'))
manual={3:[(3,[(0,0,0x260),(1,4,0xf0)],'FindingAlys')],6:[(6,[(1,0,0xad0),(0,4,0x710),(0,1,0x750)],'MachineCenter')],12:[(0x12,[(1,0,0x180)],'BioPlantAlarm')],0x1a:[(0x8007,[(1,0,0x1a0)],'MeetingRika')],0x1c:[(0x8008,[(1,0,0x170)],'SavingDemi')],0x1d:[(0x8009,[],'AlysWounded')],0x1f:[(0x34,[(1,0,0x1e0)],'ZioNurvus')],0x35:[(0x4c,[(1,1,0xc0),(0,4,0xba0),(0,1,0xbb0)],'CarnivorousTrees'),(0x4d,[(1,1,0xc0),(0,4,0xba0),(0,1,0xbb0)],'SavingKyra'),(0x8013,[],'MeetingKyra')],0x51:[(0x69,[(0,4,0x1c0),(0,1,0x1d0),(1,4,0x250),(1,1,0x260)],'AngerTowerTop')],0x52:[(0x6a,[(1,0,0x260),(0,4,0x1a0),(0,1,0x1b0)],'AngerTowerExit')],0x53:[(0x8020,[(1,0,0x160)],'ProfoundDarkness')],0x70:[(0x71,[(0,1,0x170),(1,4,0x150),(1,1,0x2b0)],'MileSandWorm')],0x7b:[(0x96,[(1,0,0x260)],'PenguinFeed')],0xa:[(0xf,[],'SuspicionOnPrincipal')],0x55:[(0x2a,[(1,0,0x310),(0,4,0x1e0),(0,1,0x1f0)],'CancellerReminder')],
 0xd:[(0x14,[],'RidingElevator')],
 0xe:[(0x15,[(0,0,0x2c0),(1,0,0x210)],'Vahal platform 1'),(0x16,[(0,0,0x2c0),(1,0,0x390)],'Vahal platform 2')],
 0xf:[(0x17+n,[(0,0,x),(1,0,y)],'Weapon platform') for n,(x,y) in enumerate([(0x1c0,0x290),(0x220,0x150),(0x380,0x150),(0x3e0,0x290)])],
 0x10:[(0x1d,[(0,0,0x180),(1,0,0x210)],'Vahal conveyor down'),(0x1e,[(0,0,0x440),(1,0,0x210)],'Vahal conveyor up'),(0x1f,[(0,0,0x1c0),(1,0,0x3d0)],'Vahal conveyor right'),(0x20,[(0,0,0x3a0),(1,0,0x3d0)],'Vahal conveyor left')]
 }
conditions.update(manual)
C=collections.defaultdict(list)
def add(ev,id_,x,y,face,why,priority):
 if ev<=0xa0 or ev&0x8000:
  C[ev].append({'map':id_,'x':x,'y':y,'facing':face,'source':why,'priority':priority})
cache={}
def offset(data,p,n):
 for _ in range(n):p=data.index(255,p)+1
 return p

def ev_refs(data,entry):
 p=offset(data,0,entry);seen=set();todo=[p];refs=set()
 while todo:
  p=todo.pop()
  if p in seen or p+2>=len(data):continue
  seen.add(p)
  if data[p]==0xfa:
   todo.extend([p+3,offset(data,p+2,data[p+2])])
  elif data[p]==0xf6:refs.add(int.from_bytes(data[p+1:p+3],'big'))
 return refs
for id_,v in maps.items():
 for p in range(v[13],v[13]+10240,10):
  if word(p)==65535:break
  if rom[p+8]==2:
   ev=word(0x64f22+rom[p+9]*2);add(ev,id_,word(p)*8,word(p+2)*8+16,1,f'area interaction {p:06X}',0)
 start=v[14];end=rom.index(255,start)
 for run in rom[start:end]:
  for ev,cmps,why in conditions[run]:
   x,y,face=default(id_)
   for ax,op,value in cmps:
    old=[x,y][ax]
    if op==0:next_=value
    elif op in (1,3):next_=min(old if old is not None else value,value-(op==3))
    else:next_=max(old if old is not None else value,value+(op==2))
    if ax==0:x=next_
    else:y=next_
   add(ev,id_,x,y,face,f'RunEvent {run:02X}: {why}',0 if cmps else 3)
 tree=v[12]
 if tree not in cache:cache[tree]=kosinski_decompress(rom[tree:])
 data=cache[tree]
 for p in range(v[7],v[7]+10240,10):
  if word(p)==65535:break
  entry=rom[p+3]
  try:refs=ev_refs(data,entry)
  except (ValueError,IndexError):continue
  for ev in refs:add(ev,id_,word(p+6)*8,word(p+8)*8+16,1,f'NPC {p:06X}, dialogue {entry:02X}, tree {tree:06X}',1)
labels=re.findall(r'    "([^"]*)",',(S/'gba/r28_debug_names.h').read_text().split('r28_event_names')[1].split('};')[0]);mlabels=re.findall(r'    "([^"]*)",',(S/'gba/r28_debug_names.h').read_text().split('r28_map_names')[1].split('};')[0])

# These scene origins are explicit in the pinned executor/source, rather than
# current-screen defaults. Context-dependent vehicle/recovery/inn roots stay
# unavailable; they have no unique physical location in the original game.
for ev,mid,x,y,face,why in [
 (7,0,0x340,0x5a0,1,'Event_MotaSpaceportAppearing source center'),
 (0x11,1,0xc0,0x490,1,'Event_DezoSpaceportAppearing source center'),
 (0x97,0x5e,0x280,0x230,3,'Event_Pennant ChazHouse scene'),
 (0x98,0x5e,0x280,0x230,3,'Event_WoodCarving ChazHouse scene'),
 (0x9f,0x5e,0x220,0x1c0,0,'Event_GameStart first RefreshMap')]:add(ev,mid,x,y,face,why,-1)
for ev,source_event in [(0x37,0x35),(0x38,0x36)]:
 c=min(C[source_event],key=lambda c:(c['priority'],c['map']))
 add(ev,c['map'],c['x'],c['y']+(16 if ev==0x37 else -16),c['facing'],'authored Garuberk door interaction origin',-1)
# After-battle roots start alongside the same combat actor, not at a distant
# map entrance. This is especially important for camera/party choreography.
for ev,source_event in [(0x25,0x6b),(0x41,0x40),(0x55,0x54),(0x57,0x56),(0x64,0x60),(0x65,0x61),(0x72,0x70),(0x7e,0x7d),(0x89,0x88)]:
 c=min(C[source_event],key=lambda c:(c['priority'],c['map']))
 add(ev,c['map'],c['x'],c['y'],c['facing'],'same authored scene as event %02X'%source_event,-1)
selected=[];missing=[]
for i,name in enumerate(labels):
 cs=sorted(C.get(i,[]),key=lambda c:(c['priority'],c['map']))
 valid=[c for c in cs if c['x'] is not None and c['y'] is not None]
 if valid:
  c=valid[0].copy();c.update(event=i,name=name,map_name=mlabels[c['map']]);selected.append(c)
  assert c['map'] in maps and 0<=c['x']<=(maps[c['map']][20]+1)*32 and 0<=c['y']<=(maps[c['map']][21]+1)*32, c
 else:missing.append({'event':i,'name':name,'reason':'no fixed authored physical origin located; never execute on current screen'})
byid={c['event']:c for c in selected}
out=['/* Generated from pinned map triggers, NPC dialogue branches and scene origins. */',
 'struct R46DebugEventLocation {uint16_t map,x,y;uint8_t facing,reserved;};',
 'static const struct R46DebugEventLocation r46_debug_event_locations[160]={']
for i in range(160):
 c=byid.get(i);out.append((' {%du,%du,%du,%du,0u},'%(c['map'],c['x'],c['y'],c['facing']) if c else ' {0xFFFFu,0u,0u,0u,0u},')+' /* %02X %s */'%(i,labels[i]))
out.append('};')
(S/'gba/r46_debug_event_locations.h').write_text('\n'.join(out)+'\n')
report={'event_count':160,'located_events':len(selected),'unlocated_events':missing,'selected':selected,'candidates':C,
 'scope':'Map triggers, original NPC F6 branches and explicit scene origins. Multiple valid origins use the first canonical source record. Flags, inventory and party prerequisites are not reset. All scenes have not been replayed.'}
(S.parent/'build_artifacts/R46_RELEASE07_DEBUG_EVENT_LOCATIONS.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS source event locations:',len(selected),'located;',len(missing),'without fixed origin')
print('Unlocated:',','.join('%02X'%c['event'] for c in missing))
