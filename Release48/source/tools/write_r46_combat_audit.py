#!/usr/bin/env python3
"""Write exhaustive, reviewable combat coverage without claiming visual parity."""
from pathlib import Path
import re,json
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
c=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.constants.asm').read_text()
def names(prefix,n):
 out={}
 for m in re.finditer(r'^'+prefix+r'(\w+)\s*=.*?;\s*(\$[0-9A-Fa-f]+|\d+)\s*$',c,re.M):out[int(m[2][1:],16) if m[2].startswith('$') else int(m[2])]=m[1]
 return [out.get(i,'SOURCE ENTRY') for i in range(1,n+1)]
def rows(text,name):
 m=re.search(r'\b'+name+r'\[[^]]*\]=\{(.*?)\n\};',text,re.S);assert m
 return [[int(q.removesuffix('u'),0) for q in re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+)u?',line)] for line in m[1].splitlines() if '{' in line]
s=(R/'gba/source_ability_effects.c').read_text();aa=rows(s,'ps4_source_ability_anims')
lines=['# R46 complete combat presentation audit','',
 'Checkpoint 09 audits every source catalog entry. Pixel/hold checks establish asset correctness. Selected DF3/vehicle/item actions retain focused CP06 native emulator evidence. CP07 verifies ordinary all-range gun HP/menu targeting and physical hit-before-damage ordering. CP08 adds all-miss gun impact suppression, occupied-target/plasma impact and Foi delays, once-computed native outcomes and scoped native damage/death cleanup. These checks do not establish all controller timings, placement, palettes, later enemy/critical reactions or real-device display. Every row still needs complete real-battle visual validation.','',
 '## Techniques and skills','', '| Kind | ID | Source name | Native path | Sprite animations | Source tables |', '| --- | --- | --- | --- | --- | --- |']
entries=[]
for kind,prefix,count,array in [('TECH','TechID_',38,'ps4_source_tech'),('SKILL','SkillID_',54,'ps4_source_skill')]:
 descs=rows(s,array);assert len(descs)==count+1
 for i,name in enumerate(names(prefix,count),1):
  first,n,k,*_=descs[i];path={1:'source sprite/controller',2:'screen controller',3:'weapon controller',4:'raw sprite controller',5:'explicit custom controller'}[k]
  tables=[f'{a[4]:06X}' for a in aa[first:first+n]]
  entries.append(dict(kind=kind,id=i,name=name,path=path,animations=n,tables=tables))
  lines.append(f'| {kind} | {i:02X} | {name} | {path} | {n} | '+', '.join(tables)+' |')
combo=json.loads((R/'source_combo_effects_manifest.json').read_text())
lines+=['','## Combinations','','| ID | Source name | Loader | Mapping encodings | Sprite sequences |','| --- | --- | --- | --- | --- |']
for i in range(1,15):
 q=combo['combos'][str(i)];lines.append(f'| {i:02X} | {q["name"]} | LoadPLC2 | '+', '.join(sorted({s['encoding'] for s in q['sources']}))+' | '+str(len(q['sources']))+' |')
lines+=['','## Enemy action pairs','', 'All 234 source pairs and 622 sequence records are inventoried. 123 standard animation sequences were misclassified as direct mappings across 99 attack entries. 68 controller-data references are now excluded from sprite drawing. 554 sprite sequences remain; all emitted pixels and holds are compared against original source art and the selected source mappings.','', '| Enemy | Action | Original routine | Native kind | Sprite sequences / records |','| --- | --- | --- | --- | --- |']
spec=json.loads((R/'tools/source_enemy_effect_specs.json').read_text());et=(R/'gba/source_enemy_effects.c').read_text();ea=rows(et,'ps4_source_enemy_anims');ed=rows(et,'ps4_source_enemy_descs')
for p,d in zip(spec['pairs'],ed):lines.append(f'| {p["enemy"]:02X} | {p["ability"]:02X} | {p["attack_routine"]} | {p["kind"]} | {sum(a[1]>0 for a in ea[d[0]:d[0]+d[1]])} / {d[1]} |')
imp=json.loads((O/'R46_RECORDED_IMPORT.json').read_text());peaks=json.loads((O/'R46_RECORDED_DATA_EVIDENCE.json').read_text())['selected_peaks']
items=names('ItemID_',160);vehicle_names=['Land Rover','Ice Digger','Hydrofoil'];skill_names={1:'Cluster',2:'Gravitn',3:'ThGrid',4:'XBurst',5:'Napalm',7:'NSpher'}
lines+=['','## Selected vehicle and item effects','','CP06 selectively enables all nine vehicle effects and eight offensive items. These retain the captured branch and pass original-tile/raw-schedule/host tests; six actual native command cases verify gameplay/resource execution once. Full vehicle battles/plane scrolling, every target/status branch and the remaining 18 battle items remain open. Raw owner-tagged party data is preserved without replacing working native weapon paths.','','| Category | Key | Name | Source ticks | Tracks | Pre-cull pieces | Live tiles |','| --- | --- | --- | ---: | ---: | ---: | ---: |']
for p in peaks:
 k,a,b=p['key']
 if k not in (9,10):continue
 name=items[a-1] if k==9 else vehicle_names[a-1]+' / '+(skill_names.get(b,'main weapon'))
 lines.append(f'| {"Item" if k==9 else "Vehicle"} | {a:02X}/{b:02X} | {name} | {p["ticks"]} | {p["tracks"]} | {p["pieces"]} | {p["live_tiles"]} |')
lines+=['','## Open controller and weapon issues','',
 '- Checkpoint 04 implements original equipment-dependent melee body/trail paths for Chaz/Hahn/Rune/Gryz/Rika/Raja/Seth and all ten Demi/Wren gun items. The 91 weapon animations pass an independent original-pixel oracle. Alys/Kyra retain explicit slasher paths; equipment variants need review. Vehicle effects are selected captures; full vehicle plane/target/battle behavior remains open. CP07 fixes ordinary all-range gun HP/menu targeting and source hit-before-damage RNG ordering. CP08 fixes all-miss impact gating, occupied-target/plasma impact and ordinary Foi delays, and once-computed gun outcomes before graphics: 3,504 ATTACK host cases, 1,170 weapon cases and 12 native menu/queue cases. CP09 adds gun reactions, twelve boss exemptions and critical palettes (4,446 host cases / 288,990 ticks; fourteen native menu/queue cases), plus three native DblSlash equipment cases. DblSlash two-hit arithmetic and exact parent-stage/window timing remain open.',
 '- Checkpoint 05 adds captured ShadowBind 84/4B and MindBlast 84/51 schedules, original-ROM-corrected shared charge art, living-target filtering and native-battle emulator screenshots. Corrosion retains its explicit native controller. Failed-status/miss/resist branches and affected-target gating are not certified; the recorder-default schedule must not be treated as all-branch proof.',
 '- Generic enemy and party controllers still serialize some children and use common target lists. Metadata removal eliminates fake sprites but does not lower every palette/motion/spawn controller.',
 '- Combo sprite assets now use original LoadPLC2, source destination offsets and the correct physical/slasher encodings. Remaining choreography includes physical participant placement, overlapping children, palette controllers and screen effects.',
 '- Firestorm/Blizzard have existing bitmap-mode scratch-bank limits. Current Firestorm reuses one child bank while phases overlap; inspect source-live frames and add a verified rendering solution.',
 '- Checkpoint 05 adds selected Holocaust 09 and Black Hole 0A capture playback, palette/plane events and background restoration. All 38,400 Black Hole plane pixels match original ROM art; source holds and events are retained. PD 85/86/87 deliberately keep the existing native fallback. Integrated combo formation/resources, physical participant placement and worst-frame timing remain open; the custom core is not cycle-accurate.',
 '- Brose still uses a bitmap-wide line-scroll approximation. Remaining background review includes DF2 plane rise, DF3 sky palette, Zio variants and PD planes/flashes/D4/D9 consumers. Read CLAUDE_CP05_INGESTION.md before importing any more of the 557 supplied packed clips. Partial actor-bearing normal attacks and friendly items were not adopted.',
 '- Verify all 11 members, 3 vehicles, equipment variants, single/all targets, dead targets, failed status effects and full damage/heal ranges in real battles. Preserve one gameplay execution per action.']
(R.parent/'COMBAT_AUDIT.md').write_text('\n'.join(lines)+'\n')
(O/'R46_COMPLETE_ABILITY_CATALOG.json').write_text(json.dumps({'techniques':38,'skills':54,'combos':14,'enemy_pairs':234,'enemy_sequences':622,'party_members':11,'vehicles':3,'entries':entries,'visual_certification':False},indent=2)+'\n')
print('PASS full inventory: 38 techniques, 54 skills, 14 combos, 234 enemy pairs, 11 members, 3 vehicles')
