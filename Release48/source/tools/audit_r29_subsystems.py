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
q=func('field_dispatch_event_index_inner');events|={i for i in eq(q,'event_index') if i<0x8000};cuts={i&0x7fff for i in eq(q,'event_index') if i&0x8000}
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
audit={'method':'Static source dispatch/data audit plus focused presentation probes; no full playthrough. A handled ID is a code path, not behavior certification. No SHA256 calculated.',
 'valid_maps':sum(p['valid'] for p in plans['plans']), 'map_slots':417,
 'event_dispatch':{'source_count':161,'covered':len(events),'missing':missing(set(range(161)),events,event_names)},
 'cutscenes':{'real_source_count':33,'covered_real':len(cuts),'missing':missing(set(range(1,34)),cuts,cut_names)},
 'run_conditions':{'source_count':128,'used_count':len(used_runs),'covered_used':len(used_runs&runs),'missing_used':missing(used_runs,runs,run_names)},
 'map_managers':{'used_count':len(used_managers),'covered_used':len(used_managers&managers),'native_cases':len(managers),'missing_used':sorted(used_managers-managers)},
 'map_updates':{'used_count':len(used_updates),'covered_used':len(used_updates&updates),'active_entries':plans['active_update_entries'],'authored_entries':plans['authored_update_entries'],'missing_used':missing(used_updates,updates,update_names)},
 'dead_end_generated_routes':[dict(r,target=cut_names[r['event_index']&0x7fff] if r['event_index']&0x8000 else event_names[r['event_index']]) for r in routes if (r['event_index']&0x7fff not in cuts if r['event_index']&0x8000 else r['event_index'] not in events)],
 'finishable':False,'finishability_reason':'Several ordinary events (RuneFlaeli, GettingLandRover, MeetingDorin) are absent. Cutscene_AlysWounded (9) cannot dispatch after rescuing Demi; later missing cutscenes include Profound Darkness (32) and Ending (33). Additional missing map updates, tile animations, field status/teleport and controllers remain.'}
sections=[
 ('Launcher / boot / title','Present; R15 shell preserved','ARM7 native module handoff; build checks donor bytes and 24 MiB slot. No launcher changes.'),
 ('Maps / tiles / palettes / decompression','Present; some animation gaps','361 valid authored maps of 417 slots; native predecoded art, source collision/layouts, per-line scroll. Tile-animation records are skipped by map parsing; seven source TileAnimJmpTbl routines have no runtime executor.'),
 ('Map managers','Dispatch coverage complete for used IDs',f"{audit['map_managers']['covered_used']}/{len(used_managers)} used IDs, {len(managers)} native cases. This does not certify every manager side effect."),
 ('Map updates / moving floors / conveyors / tower chests','INCOMPLETE',f"{len(used_updates&updates)}/{len(used_updates)} used IDs handled including source no-op 0. {len(used_updates-updates)} used updates missing; conveyors, Strength/Courage tower chests, palette/state updates need lowering. Only {plans['active_update_entries']}/{plans['authored_update_entries']} authored entries are in active update plans."),
 ('Field objects / NPCs / interactions','Partial','221 source object routines; native generic carriers and selected motion/geometry overrides. Unhandled types reuse a 16x32/default-motion carrier. Mechanical and composite actor parity is not established.'),
 ('Collision / transitions / camera','Present; traversal gaps','930 compiled transition records and source collision, bounds, NPC blocking. Map policy flag reload fixed in R29. Moving floors/conveyors and world-vehicle traversal need source-controller work.'),
 ('Map-triggered events','Conditions mostly present; destinations blocked',f"{len(used_runs&runs)}/{len(used_runs)} map-used condition IDs recognized; {len(audit['dead_end_generated_routes'])} generated routes target absent executors/cutscenes (includes routes not used by maps)."),
 ('Event executors / guild quests / services','Partial',f"{len(events)}/161 IDs have native/generated dispatch. See exact missing IDs below. Shops, inn/recovery, inventory rewards, guild dialogue and selected quest choreography exist; full quest completion not proven."),
 ('Story / recruitment / cinematics','BLOCKS COMPLETION',f"{len(cuts)}/33 real cutscenes can dispatch. Indices 9 through 33 are absent. Confirmed cutscene blocker: AlysWounded. Missing ordinary executors can block earlier (RuneFlaeli, GettingLandRover, MeetingDorin). A source F7 callback or a flag condition does not implement its destination."),
 ('Final battle / ending / credits','MISSING story path','The Edge RunEvent 0x53 dispatches 0x8020, which has no cutscene body. RunEvent 0x54 routes to absent 0x8021. Final-boss assets/debug fights do not supply the ending; game cannot be certified finishable.'),
 ('Party / stats / EXP / learning','Present','11 character records and source 22-byte level tables; rewards invoke level-up and source skill/technique learning. R28 level-60 debug party remains. Level-up presentation/source edge cases not newly exercised.'),
 ('Battle formations / bosses / enemy AI','Present with controller gaps','27 event formations, random region groups, 153 enemy visual records; 20 AI condition cases. Source palettes, immutable native art/maps, generic idle/death controllers exist. R29 protects action banners for all boss bodies and target windows. Profound Darkness story/phase transitions require further validation.'),
 ('Party combat sprites / weapons','Body assets checked; weapon coverage partial','11 party sheets, 43 body poses and 11 close-range frames checked. Demi/Wren gun-bank replacement corrected; Wren load map has all seven columns at 42x34. Dedicated physical controllers cover Chaz/Hahn/Rika, slashers and Demi Impacter; remaining ordinary weapon attacks can take a five-frame static-pose path.'),
 ('Battle damage / statuses / target policies','Present; parity not fully certified','Native source damage/chance, elements, status effects, resource consumption, enemy mutation, macros, turn order. No broad regression sweep or complete boss playthrough performed.'),
 ('TECH / SKILL / combos','Assets present; controller behavior partial','38 TECH and 54 SKILL assets; 14 playable combo descriptions (15-entry bank includes index 0). Dedicated Foi/heal, Vortex, DblSlash, Disrupt, Death, Earth, Vision paths coexist with generic fixed-point playback. R19 controller audit remains relevant for party-reposition and movement-heavy skills.'),
 ('Vehicles / space travel','Partial','3 source vehicle stat/Optin/cockpit assets and debug fights; R29 restores world-vehicle encounter groups 8/9/10/13. Full vehicle weapon controllers and overworld movement/boarding/disembarking are not certified. Spaceship cutscenes D/E/F and associated transitions absent.'),
 ('Field ITEM / TECH / teleport','INCOMPLETE','Heal, cure, revive and full-revive paths exist. Hinas/Ryuka and Escapipe/Telepipe do not have complete field selectors/transition controllers; special items rejected as CAN\'T USE HERE or NO FIELD EFFECT.'),
 ('Field poison / paralysis / android regeneration','MISSING source frame controller','No native equivalent of UpdateCharStats field loop for poison cadence/death, paralysis recovery and android passive healing. R29 reloads the authored poison enable byte, but does not claim that supplies its missing consumer.'),
 ('Menus / dialogue / fonts / portraits','Present; readability fixed','Accepted R28 menu font/layout retained. R29 dialogue/choice uses hinted 6x11 masks with source byte encoding, 32 columns and source page boundaries; item feedback shares hinted letters. 39 portrait assets. RUN border and boss action-window layer fixed.'),
 ('Music / SFX / transport','Accepted R28, preserved','52 music IDs, 67 ordinary plus 3 special SFX; same R28 source audio files, wave banks and transport. No sound changes, synthesis tuning or extra audio tests.'),
 ('SRAM save / load / options / game over','Present; end-to-end unverified','Three source 0xA00-byte images/checksum and PSC SRAM lane/offset; Continue, slot delete, settings, defeated-to-title exist. R29 does not alter save layout; no new destructive/save soak test.'),
 ('Debug / warp / test battle / flags','Present','Named map list, event/story flag views, lvl60 Chaz/Rika/Rune/Demi/Kyra, 27 bosses and three vehicles retained. Debug flags can expose unfinished content and are not proof of story completion.'),
 ('Timing / memory / performance','Improved; hardware timing unverified','Canonical composition retires stale generic animation dirtiness, preventing an unnecessary second boss redraw and stale VRAM-window restore. Source animation holds remain; no frame skip or sped-up timers. ARM build/map/stack limits checked separately.')]
audit['subsystems']=[{'name':n,'status':st,'findings':f} for n,st,f in sections]
(O/'R29_SUBSYSTEM_AUDIT.json').write_text(json.dumps(audit,indent=2)+'\n')
lines=['# R29 current subsystem and finishability audit','', '**The game is not yet finishable.** This is a current-source audit, not a verified playthrough. The first missing cutscene by table index is AlysWounded (9); Profound Darkness (32) and Ending (33) are also absent. Ordinary event gaps can block earlier, including RuneFlaeli, GettingLandRover and MeetingDorin.','',audit['method'],'','| Subsystem | Status | Evidence / limitation |','|---|---|---|']
lines += ['| '+n+' | '+st+' | '+f.replace('|','/')+' |' for n,st,f in sections]
for key,title in [('cutscenes','Missing cutscenes'),('event_dispatch','Missing ordinary event executors'),('run_conditions','Missing map-used run conditions'),('map_updates','Missing map-used updates')]:
 rows=audit[key].get('missing',audit[key].get('missing_used',[]));lines+=['','## '+title,'','| ID | Source routine |','|---|---|']+['| $'+r['hex']+' | '+r['source']+' |' for r in rows]
lines+=['','## Repairs in this checkpoint','', '- RUN now fits within the main options window.', '- Correct Demi/Wren replacement gun art; seven-column Wren load-gun mapping retained.', '- Boss action text/target UI protected during idle redraw, including Zio/Dark Force.', '- Avoid stale queued bitmap animation replay after a full scene compose; retain source holds.', '- Readable dialogue and Yes/No/item feedback while retaining source text/page encoding.', '- Restore authored random-encounter/poison/teleport map-policy bytes and Dark Force 2 arena suppression.', '- Restore vehicle world encounter groups and Ladea/Air Castle saved-music update.', '- Audio and SRAM layout preserved.','', '## Next dependencies in source order','', '1. Implement missing early ordinary events (RuneFlaeli, MeetingDorin, GettingLandRover), then AlysWounded, PsycoWand and ZioDefeated cutscene state, choreography, panels and map continuations; then validate that chapter boundary.', '2. Implement spaceship/recruits and each remaining authored cutscene; no flag-skipping completion shortcut.', '3. Complete missing field map/object/status/teleport and vehicle controllers, and validate traversal/quests.', '4. Validate Profound Darkness phases and the authored ending/credits through a normal save.', '', 'ROM capacity alone does not implement these controllers. Assets/data coverage and debug access must not be reported as whole-game completion.']
(O/'R29_SUBSYSTEM_AUDIT.md').write_text('\n'.join(lines)+'\n')
for k in ['event_dispatch','cutscenes','run_conditions','map_managers','map_updates']:
 q=audit[k];print(k,{a:b for a,b in q.items() if a not in ('missing','missing_used')})
print('FINISHABLE: NO; source-blocker lists written,',len(audit['dead_end_generated_routes']),'unresolved generated routes')
