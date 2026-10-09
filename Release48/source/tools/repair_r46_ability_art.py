#!/usr/bin/env python3
"""Re-lower existing ability frames from each mapping's own source Nemesis bank.

The earlier generator concatenated an ability's PLC chunks, then treated every
child mapping's tile zero as the caster bank's tile zero. Preserve descriptors,
geometry, holds and explicit raw controllers; correct only this selected bank.
No hashes or unrelated asset regeneration.
"""
from pathlib import Path
import json,re
from md_compression import nemesis_decompress
from generate_enemy_animation_frames import Rom
from generate_technique_objects import plc_chunks,anim_table
from generate_source_ability_effects import TECH_ANIMS,SKILL_ANIMS,render_indices,pack_canvas,art_pool
from generate_enemy_attack_frames import records

R=Path(__file__).resolve().parents[1]
raw=(R/'references/roms/ps4built.bin').read_bytes();assert len(raw)==0x300000
rom=Rom(raw)
# Ambiguous/composite cases are established by original PLC and child loaders.
# Foi/Wat chunk order is also independently recorded by generate_technique_objects.
OVERRIDES={0x25B008:0x24418C,0x25B0C8:0x24418C,0x25B2B4:0x2442C4,
 0x25B7AE:0x24468A,0x25B968:0x2449D2,0x25D39C:0x246FC0,
 0x25D916:0x24971E,0x25B62E:0x244390,
 # Death loads PLC C4 at $9B80 before its hit child, independently of A4.
 0x25EB44:0x24E2B4}
refs={}
for kind,tables in [('T',TECH_ANIMS),('S',SKILL_ANIMS)]:
 for aid,ts in tables.items():
  chunks,ptrs,counts=plc_chunks(rom,aid+(0x4F if kind=='T' else 0x8C))
  for table in ts:
   if table in (0x25FDDE,0x25FDF0,0x25F7AC,0x2608EE):continue
   maps,_=anim_table(rom,table)
   need=max((p&0x7FF)+w*h for m in maps if m for _,_,w,h,p in records(m))
   refs.setdefault(table,[]).append((set(p for p,n in zip(ptrs,counts) if n>=need),set(p for p,n in zip(ptrs,counts) if n==need)))
sources={}
for table,rows in refs.items():
 valid=set.intersection(*(q[0] for q in rows));exact=set.union(*(q[1] for q in rows)) & valid
 if table in OVERRIDES:sources[table]=OVERRIDES[table]
 elif len(exact)==1:sources[table]=next(iter(exact))
 elif len(valid)==1:sources[table]=next(iter(valid))
 else:raise ValueError(('Unresolved source art',hex(table),valid,exact))

path=R/'gba/source_ability_effects.c';text=path.read_text();old=(R/'gba/source_ability_effects.bin').read_bytes()
def rows(name):
 m=re.search(r'\b'+name+r'\[[^]]*\]=\{(.*?)\n\};',text,re.S);assert m,name
 return m,[[int(q.removesuffix('u'),0) for q in re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+)u?',line)] for line in m[1].splitlines() if '{' in line]
part_match,parts=rows('ps4_source_ability_parts');anim_match,anims=rows('ps4_source_ability_anims')
assert len(parts)==167 and len(anims)==132
bank=bytearray();cache={};changes=[];preserved=[];payloads=[]
def put(payload):
 key=bytes(payload)
 if key in cache:return cache[key]
 while len(bank)%4:bank.append(0)
 offset=len(bank);bank.extend(payload);cache[key]=offset;return offset
def dimensions(geom):
 shape,size=geom&3,(geom>>2)&3
 return ([(8,8),(16,16),(32,32),(64,64)],[(16,8),(32,8),(32,16),(64,32)],[(8,16),(8,32),(16,32),(32,64)])[shape][size]
art_cache={}
for ai,a in enumerate(anims):
 first,n,nframes,hoff,table=a;holds=old[hoff:hoff+nframes];maps=None;art=None
 if table in sources:
  maps,source_holds=anim_table(rom,table);assert len(maps)==nframes and bytes(source_holds)==holds,(hex(table),nframes)
  pointer=sources[table]
  if pointer not in art_cache:art_cache[pointer]=nemesis_decompress(rom.tail(pointer))
  art=art_cache[pointer]
 elif table==0x2608EE:
  # High A4 projectile tiles survive the low C4 overlay exactly as in source.
  art=art_pool(rom,'S',0x18);maps,source_holds=anim_table(rom,table);assert bytes(source_holds)==holds
 else:preserved.append(hex(table))
 for pi in range(first,first+n):
  p=parts[pi];off,frame_bytes,geom,reserved,ox,oy,pad=p;w,h=dimensions(geom);assert frame_bytes==w*h//2
  original=old[off:off+frame_bytes*nframes]
  if maps is None:payload=original
  else:
   frames=[]
   for m in maps:
    if m is None:frames.append(bytes(frame_bytes));continue
    pix,pw,ph=render_indices(art,m,(ox,oy,ox+w,oy+h));assert (pw,ph)==(w,h)
    frames.append(pack_canvas(pix,w,h,w,h))
   payload=b''.join(frames)
  assert len(payload)==len(original)
  if payload!=original:changes.append({'animation':ai,'part':pi,'table':f'{table:06X}','source_nemesis':f'{sources[table]:06X}' if table in sources else 'A4+C4 retained high projectile','bytes':len(payload)})
  p[0]=put(payload);payloads.append((pi,payload))
 a[3]=put(holds)
for pi,payload in payloads:assert bytes(bank[parts[pi][0]:parts[pi][0]+len(payload)])==payload
part_body='\n'+'\n'.join('{%du,%du,%du,%du,%d,%d,%du},'%tuple(p) for p in parts)
anim_body='\n'+'\n'.join('{%du,%du,%du,%du,0x%06Xu},'%tuple(a) for a in anims)
# Replace from the back so retained match offsets remain authoritative.
for m,body in sorted([(part_match,part_body),(anim_match,anim_body)],key=lambda q:q[0].start(1),reverse=True):text=text[:m.start(1)]+body+text[m.end(1):]
path.write_text(text);(R/'gba/source_ability_effects.bin').write_bytes(bank)
report={'result':'PASS','old_bytes':len(old),'new_bytes':len(bank),'animation_records':len(anims),'part_records':len(parts),'corrected_parts':len(changes),'corrected_source_tables':len(set(q['table'] for q in changes)),'unchanged_explicit_raw_tables':sorted(set(preserved)),'source_art_registry':{f'{k:06X}':f'{v:06X}' for k,v in sorted(sources.items())},'changes':changes,'scope':'Selected ability-bank pixels corrected against source Nemesis art/mappings; descriptors, holds and all unrelated banks retained. Controller scheduling is validated separately.'}
(R.parent/'build_artifacts/R46_ABILITY_ART_REPAIR.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS ability art:',len(changes),'parts corrected;',len(old),'->',len(bank),'bytes;',len(set(q['table'] for q in changes)),'source tables')
