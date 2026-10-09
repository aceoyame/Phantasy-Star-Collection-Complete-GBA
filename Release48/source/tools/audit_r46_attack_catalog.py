#!/usr/bin/env python3
"""Source inventory for the complete native combat presentation catalog."""
from pathlib import Path
import json,re
from generate_enemy_animation_frames import Rom
from generate_technique_objects import plc_chunks,anim_table
from generate_enemy_attack_frames import records
from source_ability_art_sources import SOURCE_ABILITY_ART

R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
raw=(R/'references/roms/ps4built.bin').read_bytes();rom=Rom(raw)
asm=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text(encoding='latin1')
spec=json.loads((R/'tools/source_enemy_effect_specs.json').read_text())
pat=r'^(\w+):\s*\n\s*dc\.w\s+(\$[0-9A-Fa-f]+|\d+)\s*\n(?:\w+:\s*\n)?\s*dc\.w\s+\w+-\w+'
standard_labels={m[1] for m in re.finditer(pat,asm,re.M)}
STANDARD={int(s[4:],16) for s in standard_labels if re.fullmatch(r'loc_[0-9A-Fa-f]+',s)}
STANDARD.update(SOURCE_ABILITY_ART)
# Named standard tables such as Map_Earth2 use the authoritative listing PC.
for line in (R/'references/disassembly/ps4disasm/ps4disasm/ps4.lst').open(encoding='latin1'):
    m=re.match(r'\s*\d+/\s*([0-9A-Fa-f]+)\s*:\s*(\w+):\s*$',line)
    if m and m[2] in standard_labels:STANDARD.add(int(m[1],16))

def source_bank(table,plcs):
    maps,holds=anim_table(rom,table)
    lo=min((p&0x7ff) for m in maps if m for _,_,w,h,p in records(m))
    high=max((p&0x7ff)+w*h for m in maps if m for _,_,w,h,p in records(m))
    ptrs=[];counts=[]
    for plc in plcs:
        _,p,n=plc_chunks(rom,plc);ptrs+=p;counts+=n
    candidates=list(dict.fromkeys((p,n) for p,n in zip(ptrs,counts) if n>=high))
    exact=[p for p,n in candidates if n==high]
    known=SOURCE_ABILITY_ART.get(table)
    if any(p==known for p,n in candidates):return known,maps,holds
    if len(exact)==1:return exact[0],maps,holds
    if len(candidates)==1:return candidates[0][0],maps,holds
    if table==0x25B23C and plcs in ([0x13],[0x8B]):
        # The backward-referenced low-tile caster frames retain the primary
        # bank used by 25B0C8; the original controller changes no PLC here.
        assert counts[0]>=high
        return ptrs[0],maps,holds
    # PD3 NSpher's child loader adds the primary PLC's 69-tile span;
    # 25ED7C/84/8C/94 therefore read the final 40-tile 24E7C2 chunk.
    if table in (0x25ED7C,0x25ED84,0x25ED8C,0x25ED94) and plcs==[0xE0]:
        assert ptrs[-1]==0x24E7C2 and counts[-1]>=high
        return ptrs[-1],maps,holds
    # Re-Faze PLC 8B: loc_284C0's child uses loc_24A20's low-bank base;
    # the second PLC chunk belongs to the later 25B62E animation. The primary
    # 30-tile Nemesis is the correct low bank for 25B0C8.
    if table==0x25B0C8 and plcs==[0x8B]:return 0x244390,maps,holds
    raise ValueError({'table':f'{table:06X}','plcs':plcs,'lo':lo,'high':high,'candidates':[(f'{p:06X}',n) for p,n in candidates]})

def audit():
    bad=[];sources={};unresolved=[]
    for pi,p in enumerate(spec['pairs']):
        for si,s in enumerate(p['sequences']):
            table=s['source_table']
            if table not in STANDARD or s['source_kind']=='body_map':continue
            if s['art_kind']!='plc':continue
            try:pointer,maps,holds=source_bank(table,s['art_plcs'])
            except ValueError as e:unresolved.append({'pair':pi,'sequence':si,'detail':e.args[0]});continue
            row={'pair':pi,'sequence':si,'enemy':f'{p["enemy"]:02X}','ability':f'{p["ability"]:02X}',
                 'table':f'{table:06X}','frames':len(maps),'ticks':sum(holds),'source':f'{pointer:06X}'}
            sources[(pi,si)]=pointer
            if s['source_kind']=='direct':bad.append(row)
    runtime=(R/'gba/title_runtime_v06.c').read_text()
    assert 'battle_r46_melee_source_attack' in runtime and 'battle_r46_gun_source_attack' in runtime
    missing_basic=[{'id':i,'name':n,'issue':'Selected captured weapon effects enabled; full source vehicle plane/target/battle integration remains open'} for i,n in [(11,'Land Rover'),(12,'Ice Digger'),(13,'Hydrofoil')]]
    report={'source_enemy_pairs':234,'source_enemy_sequences':622,'techniques':38,'skills_including_vehicle_entries':54,'combos':14,'party_members':11,'vehicles':3,
            'misclassified_animation_tables':len(bad),'pairs_affected':len({q['pair'] for q in bad}),
            'misclassified':bad,'unresolved_standard_banks':unresolved,
            'basic_attack_exceptions':missing_basic,'implemented_weapon_paths':{'melee_users':[0,2,3,4,5,8,10],'gun_users':[6,7],'gun_items':10,'animations':91,'ordinary_all_range_gun_hp_menu':True,'multi_target_hit_before_damage':True,'gun_outcomes_before_visual_once':True,'all_miss_impact_gating':True,'occupied_target_and_plasma_impact_foi_delays':True,'source_counter_gun_reactions':True,'boss_exemptions':12,'critical_palette_restoration':True,'dblslash_two_visual_passes':True,'visual_miss_critical_and_staggered_reactions_certified':False},
            'selected_capture_paths':{'enemy_actions':['84/4B ShadowBind','84/51 MindBlast'],'combos':['09 Holocaust','0A Black Hole'],'vehicle_actions':9,'offensive_items':['4E','4F','50','51','62','71','8B','8E'],'all_branches_certified':False},
            'other_controller_exceptions':['DF3 captured schedules lack complete affected-target/failed-status/miss/resist branch proof','Brose uses bitmap-wide shifts instead of original line scroll','Generic enemy sequences serialize controller children; source delays/overlaps require controller review','Generic party ability roots do not prove correct individual target/caster placement','Combo custom encodings parsed; participant placement and overlap controllers remain open','Holocaust/Black Hole captured normal-background playback retains native PD fallback; integrated combo gameplay and performance remain open'],
            'scope':'Complete source entry inventory and table/art resolution. This is an audit, not runtime visual certification.'}
    (O/'R46_ALL_ATTACKS_AUDIT.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Enemy animation tables wrongly treated as direct maps:',len(bad),'across',report['pairs_affected'],'pairs; unresolved banks:',len(unresolved))
    return sources,report

if __name__=='__main__':audit()
