#!/usr/bin/env python3
"""Source-table audit. Coverage is explicitly not proof of retail behavior."""
from pathlib import Path
import re,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';O.mkdir(exist_ok=True)
s=(R/'gba/title_runtime_v06.c').read_text();asm=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text(errors='replace');h=(R/'gba/generated_simple_events_v0982.h').read_text();rom=(R/'references/roms/ps4built.bin').read_bytes()
def func(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]
def cases(q):return {int(x,0) for x in re.findall(r'case\s+(0x[0-9a-fA-F]+|\d+)u?\s*:',q)}
def eq(q,var):return {int(x,0) for x in re.findall(r'\b'+var+r'\s*==\s*(0x[0-9a-fA-F]+|\d+)u?',q)}
def table(label,ins,n):
 a=asm.index('\n'+label+':')+len(label)+2;q=asm[a:];out=re.findall(r'^\s*'+ins+r'\s+([\w]+)',q,re.M)[:n];assert len(out)==n,(label,len(out));return out
def rowtable(text):
 types={m[1]:m[2] for m in re.finditer(r'struct (\w+) \{([^}]+)\};',text)};out={}
 for m in re.finditer(r'static const struct (\w+) (\w+)\[\d+\]=\{(.*?)\n\};',text,re.S):
  rows=[[int(x,0) for x in re.findall(r'(0x[0-9a-fA-F]+|\d+)u',r)] for r in re.findall(r'^\s*\{([^}]+)\}',m[3],re.M)]
  out[m[2]]=(types.get(m[1],''),rows)
 return out
events=set();runs=set();routes=[]
for name,(typ,rows) in rowtable(h).items():
 if re.match(r'\s*uint8_t event_index\b',typ):events.update(r[0] for r in rows)
 if re.match(r'\s*uint8_t run_id\b',typ):
  runs.update(r[0] for r in rows)
  off=2 if name=='ps4_simple_run_event_conditions' else 1
  routes.extend({'run_id':r[0],'event_index':r[off],'origin':name} for r in rows)
q=func('field_dispatch_event_index_inner');
for lo,hi in re.findall(r'event_index\s*>=\s*(0x[0-9a-fA-F]+|\d+)u?\s*&&\s*event_index\s*<=\s*(0x[0-9a-fA-F]+|\d+)u?',q):events.update(range(int(lo,0),int(hi,0)+1))
events|=eq(func('field_event_defeated_guardians'),'event_index')
events|={i for i in eq(q,'event_index') if i<0x8000};cuts={i&0x7fff for i in eq(q,'event_index') if i&0x8000}
cut_h=(R/'gba/generated_cutscene_scripts_v0982.h').read_text()
for typ,rows in rowtable(cut_h).values():
 if 'cutscene_index' in typ:cuts.update(r[0] for r in rows)
run_names=table('RunEventsJmpTbl',r'bra\.w',128);event_names=table('EventPtrs',r'dc\.l',161);cut_names=table('CutscenePtrs',r'dc\.l',34);update_names=table('MapUpdateJmpTbl',r'bra\.w',64)
for n in ['field_run_map_event_condition','field_run_cp25_event_condition']:
 q=func(n);runs|=eq(q,'id')|cases(q)
noop_runs={i for i,n in enumerate(run_names) if 'Null' in n or 'NoEvent' in n};runs|=noop_runs
plans=json.loads((R/'generated/psc_runtime_plan_report.json').read_text());used_runs={x for p in plans['plans'] for x in p['events']};used_updates={x for p in plans['plans'] for x in p['authored_updates']};used_managers={x for p in plans['plans'] for x in p['managers']}
updates=cases(func('field_apply_map_update_id'))|{i for i,n in enumerate(update_names) if 'NoUpdate' in n};managers=cases(func('field_apply_map_manager_load'))
def missing(ids,handled,names):return [{'id':i,'hex':'%02X'%i,'source':names[i]} for i in sorted(ids-handled)]
audit={'method':'Static source dispatch/data audit plus focused source-clock/event-state/OBJ probes; no full playthrough. A handled ID is a code path, not behavior certification. No SHA256 calculated.',
 'valid_maps':sum(p['valid'] for p in plans['plans']), 'map_slots':417,
 'event_dispatch':{'source_count':161,'covered':len(events),'missing':missing(set(range(161)),events,event_names)},
 'cutscenes':{'real_source_count':33,'covered_real':len(cuts),'missing':missing(set(range(1,34)),cuts,cut_names)},
 'run_conditions':{'source_count':128,'used_count':len(used_runs),'covered_used':len(used_runs&runs),'missing_used':missing(used_runs,runs,run_names)},
 'map_managers':{'used_count':len(used_managers),'covered_used':len(used_managers&managers),'native_cases':len(managers),'missing_used':sorted(used_managers-managers)},
 'map_updates':{'used_count':len(used_updates),'covered_used':len(used_updates&updates),'active_entries':plans['active_update_entries'],'authored_entries':plans['authored_update_entries'],'missing_used':missing(used_updates,updates,update_names)},
 'dead_end_generated_routes':[dict(r,target=cut_names[r['event_index']&0x7fff] if r['event_index']&0x8000 else event_names[r['event_index']]) for r in routes if (r['event_index']&0x7fff not in cuts if r['event_index']&0x8000 else r['event_index'] not in events)],
 'finishable':False,'finishability_reason':'InsideSpaceship ($0D) and SpaceshipSabotage ($0E) now have native selector/flight/landing and sabotage/battle8 paths with focused source probes. The first Rykros arrival remains blocked by original cutscene $1B. Five ordinary events, later cutscenes/controllers, Profound Darkness and ending remain unresolved. No normal-save emulator/hardware playthrough has been completed.'}

audit['r36_repairs']=['Original destination mask priority/current-world exclusion and selector art/names/palette blink; source confirm and Cancel behavior','Real-map AD continuations for takeoff/landing/final entry/cancel; 112/228/419 ascent updates, 60 white frames, 300 space frames and 257 landing updates','Original small ship/Landale/planet art and counters; 54 appended poses, complete 107712-byte R35 prefix retained','Sabotage Tree35 entry2 F7, four authored panels, ChaosSorcr flag and real battle8; unchanged map RunEvent29 owns post-victory crash','Independent 1638-frame reference and twelve source-art space pass checks; accepted audio/shell and EWRAM allocation preserved']
audit['r35_repairs']=['MeetingWren ($0C): source panel/dialogue, Wren carrier/ATTACK macro and flag, original 32-byte NPC clear','CrashLanding ($0F): original sounds/panels, Tree14 seams, three map continuations, 599-frame original 16.16 FG/BG pan and all 44 snow launches','Raja type $24 and Snow type $1F4 actors/poses; original BG door records and six-update clock; full bit-3 object preservation and 64-byte Raja join','All EWRAM objects/sizes unchanged; BSS sorted by alignment to reclaim padding; complete 72,896-byte R34 pose prefix retained']
audit['r34_repairs']=['PsycoWand (0A) original Tree6 dialogue seams, panel montage and Krup Inn/Krup continuations','ZioDefeated (0B) original Tree36 seams, preserved bit-3 same-map party reload, Demi drift, original FG elevator records, spaceport handoff, party removal and final Motavia reload','Hahn type 0C source mappings/raw-art poses; retained 68,800-byte R33 bank prefix; no new EWRAM globals']
audit['r33_repairs']=['PsycoWandChest event with source countdowns, 18 fragment slots, guardian animation-completion gates, dialogue/camera and battle 05 handoff','AlysWounded with optional Saya branch, Krup/Krup Inn reload continuations, source Tree6 F7 seams, party/carrier/macro compaction and DemiJoined flag','223 new immutable source OBJ poses; retained 26,272-byte R32 prefix; no new EWRAM globals']
audit['r31_repairs']=['RuneFlaeli full field sequence: party movement, casting actor, flame, 14 dust and 14 ballistic rock spawns, original four BG chunk edits and TonoePathOpen gate','MeetingDorin field sequence: source dialogue branch, Alys wind-up/punch, anger lines, Dorin impact/star reactions, normal actor hiding/restoration and Dorin flag','54 offline native OBJ poses including multi-piece tall flames and shared dust/rock frames','Original 16.16 debris gravity, source animation lists, actor counters and source RAM state','VRAM cache invalidation on pose reallocation; no new EWRAM globals','R30 field controllers, accepted audio, presentation, save layout and first 8 MiB PSC shell preserved']
audit['remaining_non_dispatch_work']=['In-game verification of teleport choreography and native background movement projection','Vehicle battle weapons and in-game traversal verification','Composite/mechanical field object parity outside the implemented handlers','Remaining physical weapon and movement-heavy TECH/SKILL controllers','Profound Darkness phases and the normal ending/credits path','Normal-save full playthrough and hardware timing remain unverified']
(O/'R36_SUBSYSTEM_AUDIT.json').write_text(json.dumps(audit,indent=2)+'\n')
lines=['# R36 current coverage and remaining completion blockers','','R36 continues R35 with the original spaceship selector, takeoff/space pass/landing and sabotage sequence. Accepted audio, launcher and the complete R35 actor bank prefix are retained. First Rykros arrival ($1B), later scenes/controllers and full playthrough remain open.','',audit['method'],'','| Dispatch | Covered | Total |','|---|---:|---:|',f"| Ordinary events | {len(events)} | 161 |",f"| Real cutscenes | {len(cuts)} | 33 |",f"| Map-used updates (including source no-op) | {len(used_updates&updates)} | {len(used_updates)} |",f"| Map-used conditions | {len(used_runs&runs)} | {len(used_runs)} |",f"| Map-used managers | {len(used_managers&managers)} | {len(used_managers)} |",'', 'A dispatch count indicates a native path, not certification of all choreography or controller behavior.','', '## Retained from R31','']+['- '+x for x in audit['r31_repairs']]
for key,title in [('event_dispatch','Remaining ordinary event executors'),('cutscenes','Remaining cutscenes'),('map_updates','Remaining map-used updates')]:
 rows=audit[key].get('missing',audit[key].get('missing_used',[]));lines+=['','## '+title,'','| ID | Source routine |','|---|---|']+['| $'+r['hex']+' | '+r['source']+' |' for r in rows]
lines+=['','## Remaining controller and verification work','']+['- '+x for x in audit['remaining_non_dispatch_work']]
lines+=['','## Implemented in R33','']+['- '+x for x in audit['r33_repairs']]
lines+=['','## Implemented in R34','']+['- '+x for x in audit['r34_repairs']]
lines+=['','## Implemented in R35','']+['- '+x for x in audit['r35_repairs']]
lines+=['','## Implemented in R36','']+['- '+x for x in audit['r36_repairs']]
lines+=['','## Immediate next work','','Verify a normal save through Wren, spaceship sabotage, the real Chaos Sorcerer battle, crash and Raja. Then address Landale ($10), Dark Force1 ($11), later story scenes and Rykros ($1B). Remaining ordinary executors are $2C, $46, $47, $48 and $86. Hardware frame/audio timing and full game finishability are unverified.','']
(O/'R36_SUBSYSTEM_AUDIT.md').write_text('\n'.join(lines))
for k in ['event_dispatch','cutscenes','map_updates']:print(k,{a:b for a,b in audit[k].items() if a not in ('missing','missing_used')})
