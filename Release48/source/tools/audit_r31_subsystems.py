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
 'finishable':False,'finishability_reason':'Six ordinary events remain absent. RuneFlaeli, MeetingDorin and GettingLandRover have native executors. Cutscene_AlysWounded (9) cannot dispatch after rescuing Demi; later missing cutscenes include Profound Darkness (32) and Ending (33). Six map updates, field teleport/traversal and battle controllers remain. Six non-empty tile-animation routines and the field status loop are now implemented; normal playthrough is unverified.'}

audit['r31_repairs']=['RuneFlaeli full field sequence: party movement, casting actor, flame, 14 dust and 14 ballistic rock spawns, original four BG chunk edits and TonoePathOpen gate','MeetingDorin field sequence: source dialogue branch, Alys wind-up/punch, anger lines, Dorin impact/star reactions, normal actor hiding/restoration and Dorin flag','54 offline native OBJ poses including multi-piece tall flames and shared dust/rock frames','Original 16.16 debris gravity, source animation lists, actor counters and source RAM state','VRAM cache invalidation on pose reallocation; no new EWRAM globals','R30 field controllers, accepted audio, presentation, save layout and first 8 MiB PSC shell preserved']
audit['remaining_non_dispatch_work']=['Field teleport: Hinas/Ryuka/Escapipe/Telepipe selectors and controllers','Vehicle-specific traversal, collision and weapons','Composite/mechanical field object parity outside the implemented handlers','Remaining physical weapon and movement-heavy TECH/SKILL controllers','Profound Darkness phases and the normal ending/credits path','Normal-save full playthrough and hardware timing remain unverified']
(O/'R31_SUBSYSTEM_AUDIT.json').write_text(json.dumps(audit,indent=2)+'\n')
lines=['# R31 current coverage and remaining completion blockers','','R31 continues the saved R30 checkpoint. The accepted audio and presentation are retained. Two early ordinary event blockers now have native source-derived executors. The game is not yet certified finishable.','',audit['method'],'','| Dispatch | Covered | Total |','|---|---:|---:|',f"| Ordinary events | {len(events)} | 161 |",f"| Real cutscenes | {len(cuts)} | 33 |",f"| Map-used updates (including source no-op) | {len(used_updates&updates)} | {len(used_updates)} |",f"| Map-used conditions | {len(used_runs&runs)} | {len(used_runs)} |",f"| Map-used managers | {len(used_managers&managers)} | {len(used_managers)} |",'', 'A dispatch count indicates a native path, not certification of all choreography or controller behavior.','', '## Implemented in R31','']+['- '+x for x in audit['r31_repairs']]
for key,title in [('event_dispatch','Remaining ordinary event executors'),('cutscenes','Remaining cutscenes'),('map_updates','Remaining map-used updates')]:
 rows=audit[key].get('missing',audit[key].get('missing_used',[]));lines+=['','## '+title,'','| ID | Source routine |','|---|---|']+['| $'+r['hex']+' | '+r['source']+' |' for r in rows]
lines+=['','## Remaining controller and verification work','']+['- '+x for x in audit['remaining_non_dispatch_work']]
lines+=['','## Immediate next work','','Verify the normal-save Rune/Dorin sequence in emulator/hardware. Next implement AlysWounded/PsycoWand/ZioDefeated cutscenes with complete choreography and map continuations, along with PsycoWandChest and its barrier/guardian actors. Battle controllers and later ending paths remain unfinished. Do not bypass story flags to claim completion.','']
(O/'R31_SUBSYSTEM_AUDIT.md').write_text('\n'.join(lines))
for k in ['event_dispatch','cutscenes','map_updates']:print(k,{a:b for a,b in audit[k].items() if a not in ('missing','missing_used')})
