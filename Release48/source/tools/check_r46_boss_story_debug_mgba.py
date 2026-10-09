#!/usr/bin/env python3
"""Release 07 boss captions, DF2 live palette and story-debug map handoffs.

Exercises native presentation, all 160 queue choices, eight normal map-loader
handoffs and one actual SYSTEM > DEBUG > STORY menu transaction. Scene execution
is paused at dispatch: these checks do not certify every scene's prerequisites
or complete quest/ending playthroughs.
"""
from pathlib import Path
import re,collections
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='RELEASE07'"),str(h),'exec'),globals())
report={'core':'upstream mGBA 0.10.5','rom':rom_id,'caption_cases':[],'DF2':[],'queue_cases':[],'map_handoffs':[]}
label_table=symbols['r46_debug_enemy_names'];fb=0x0202a000
source_maps={}
for row in (S/'gba/psc_map_desc.c').read_text().splitlines():
    if row.startswith('{'):
        source_maps[int(row.split('/* ')[1][:3],16)]=[int(v.rstrip('u'),0) for v in row.split('}')[0][1:].split(',')]
def prep_party():
    g.load(state);call('ps4_debug_level60_party')
    ids=list(g.mem(md+0xf40a,5));assert ids==[0,5,3,6,9]
    for i,cid in enumerate(ids):call('battle_load_char',(cid,pp+i*62))
    return ids
def scene(id_,formation=None):
    prep_party();g.put(ee,bytes(248))
    # Ordinary battle entry hides field/menu objects before composing Mode 4.
    for oi in range(128):g.wr16(0x07000000+oi*8,0x0200)
    if formation is None:
        call('battle_load_enemy',(id_,ee));g.wr32(symbols['battle_enemy_count'],1)
        g.put(symbols['battle_enemy_placement'],bytes([0x14,0,0,0]));event=255
    else:
        n=call('battle_load_event_formation',(formation,ee));assert n==1 and g.rd8(ee+20)==id_
        g.wr32(symbols['battle_enemy_count'],n);event=formation
    g.wr8(symbols['battle_event_index'],event)
    call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('r45_pd_visual_reset',(ee,))
    g.wr8(symbols['battle_party_visible_mask'],31)
    return g.mem(pp,310),g.mem(ee,248),g.mem(md+0xf100,0xa00)
def caption(id_):
    p,e,saved=scene(id_);g.wr8(symbols['battle_show_enemy_target'],1)
    call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0')
    page=g.mem(fb,38400)
    got=bytes(int(page[y*240+x]==80) for y in range(6,12) for x in range(6,144))
    assert any(got),(id_,'empty target name')
    g.put(fb,bytes(38400))
    if 0x81<=id_<=0x87:
        ptr=g.rd32(label_table+id_*4)+3
        call('battle_draw_text6_limit',(6,6,ptr,80,236))
        name=bytes(g.mem(ptr,40)).split(b'\0')[0].decode('ascii')
    else:
        ptr=0x280d42
        for _ in range(id_):ptr=original_rom.index(255,ptr)+1
        end=original_rom.index(255,ptr);name=original_rom[ptr:end].hex()
        call('battle_draw_rom_text6_limit',(6,6,ptr,255,80,236))
    want=bytes(int(g.rd8(fb+y*240+x)==80) for y in range(6,12) for x in range(6,144))
    assert got==want,(id_,'target glyphs differ from intended name')
    assert g.mem(pp,310)==p and g.mem(ee,248)==e and g.mem(md+0xf100,0xa00)==saved,(id_,'caption changed game state')
    report['caption_cases'].append({'enemy_id':id_,'readable_placeholder_override':0x81<=id_<=0x87,'name':name,'native_target_glyphs_match':True})

for id_ in range(153):caption(id_)
print('PASS all 153 native enemy target captions; seven boss placeholders corrected',flush=True)
def mdcolor(c):
    cv=[0,4,9,13,18,22,27,31]
    return cv[(c>>1)&7]|cv[(c>>5)&7]<<5|cv[(c>>9)&7]<<10
source_words=[int.from_bytes(original_rom[0x317dc+i*2:0x317de+i*2],'big') for i in range(16)]
expected=b''.join(mdcolor(c).to_bytes(2,'little') for c in source_words)
for formation in (None,0x11):
    p,e,saved=scene(0x83,formation);g.wr8(symbols['battle_show_enemy_target'],1)
    call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0');call('battle_obj_sync',(ee,0))
    line=12+(g.rd8(symbols['battle_enemy_placement'])>>7);palette=g.mem(0x05000000+line*32,32)
    assert palette==expected,'DF2 palette differs from original loc_317DC'
    page=g.mem(fb,38400);used=set(page)&set(range(line*16,line*16+16))
    colored={v for v in used if int.from_bytes(palette[(v%16)*2:(v%16+1)*2],'little')}
    assert len(colored)>=4,('DF2 still black',used,colored)
    for tick in range(48):
        call('waitvb');call('battle_action_idle_after_wait',(ee,))
        assert g.mem(0x05000000+line*32,32)==expected,('DF2 idle palette',tick)
    # Capture only after actual LCD scanout, rather than the preceding debug
    # menu frame still present immediately after the first VBlank transfer.
    shot('DF2_'+('SOURCE_FORMATION' if formation is not None else 'STANDALONE'))
    assert g.mem(pp,310)==p and g.mem(ee,248)==e and g.mem(md+0xf100,0xa00)==saved,'DF2 presentation mutated actors/resources'
    report['DF2'].append({'route':'source formation 0x11' if formation is not None else 'standalone 0x83','colored_body_indices':sorted(colored),'palette_matches_original_source':True,'idle_updates_guarded':48})
print('PASS DF2 colors in source and standalone battles, including 48 idle updates each',flush=True)

catalog=json.loads((O/'R46_RELEASE07_DEBUG_EVENT_LOCATIONS.json').read_text());locs={c['event']:c for c in catalog['selected']}
resume_names=('field_custom_resume_event','field_cutscene_resume_index','field_transition_resume_index')
for id_ in range(160):
    prep_party();g.wr16(md+0xf43c,1);w16(0xec28,0x13)
    for name in resume_names:g.wr8(symbols[name],3)
    g.wr8(symbols['field_custom_resume_phase'],2);g.wr16(symbols['ps4_debug_event_pending'],0)
    g.wr8(symbols['field_event_reload_pending'],0);g.wr8(symbols['ps4_debug_autoplace_pending'],1)
    before=g.mem(md+0xf100,0xa00);coords=g.mem(md+0xec28,36)
    handled=call('ps4_debug_queue_story_event',(id_,));assert handled==int(id_ in locs)
    if handled:
        c=locs[id_];want=bytearray(before);want[0x33c:0x33e]=bytes(2)
        assert g.mem(md+0xf100,0xa00)==want,'warp changed flags/inventory/party stats'
        assert read16(g.mem(md+0xec28,36),0)==c['map']
        assert read16(g.mem(md+0xec48,6),0)==c['x']>>3 and read16(g.mem(md+0xec48,6),2)==c['y']>>3
        assert read16(g.mem(md+0xec44,4),0)==c['facing']<<2
        assert g.rd16(symbols['ps4_debug_event_pending'])==id_+1 and g.rd8(symbols['field_event_reload_pending'])==1
        assert all(g.rd8(symbols[n])==255 for n in resume_names)
        assert g.rd8(symbols['field_custom_resume_phase'])==g.rd8(symbols['ps4_debug_autoplace_pending'])==0
    else:
        assert g.mem(md+0xf100,0xa00)==before and g.mem(md+0xec28,36)==coords
        assert g.rd16(symbols['ps4_debug_event_pending'])==g.rd8(symbols['field_event_reload_pending'])==0
        assert all(g.rd8(symbols[n])==3 for n in resume_names),'unavailable choice discarded scene'
    report['queue_cases'].append({'event':id_,'handled':bool(handled),'resource_flag_and_pending_guards':True})
print('PASS all 160 story choices: 150 authored warp destinations, ten unavailable without mutation',flush=True)

for id_ in (3,0x26,0x2f,0x4e,0x66,0x88,0x97,0x9f):
    prep_party();w16(0xec28,0x13);assert call('ps4_debug_queue_story_event',(id_,))==1;c=locs[id_]
    begin('field_runtime_loop',(c['map'],c['x'],c['y'],c['facing']))
    assert run_until_break(g,symbols['field_dispatch_event_index'],300),(id_,'loader did not dispatch')
    assert g.reg(0)==id_ and g.rd16(symbols['ps4_debug_event_pending'])==0,(id_,'wrong or repeated dispatch')
    assert read16(g.mem(md+0xec28,2),0)==c['map'],(id_,'old map still loaded')
    # Verify the loaded descriptor, including its source dialogue/object/event
    # pointers, rather than relying solely on the requested map in virtual RAM.
    desc=symbols['field_map'];want=source_maps[c['map']]
    assert g.rd16(desc)==c['map'] and g.mem(desc+2,8)==bytes(want[17:25]),(id_,'wrong loaded map geometry')
    assert g.mem(desc+12,64)==b''.join(v.to_bytes(4,'little') for v in want[:16]),(id_,'wrong loaded source map pointers')
    report['map_handoffs'].append({'event':id_,'map':c['map'],'map_name':c['map_name'],'native_loader_before_dispatch':True,'loaded_descriptor_geometry_and_all_16_pointers_match':True,'pending_consumed_once':True})
    print('PASS story handoff',f'{id_:02X}',c['map_name'],flush=True)

# Drive the actual numeric selector from SYSTEM on a loaded field.
prep_party()
for name in resume_names:g.wr8(symbols[name],255)
g.wr16(symbols['ps4_debug_event_pending'],0);g.wr8(symbols['ps4_debug_autoplace_pending'],1)
begin('field_runtime_loop',(0x13,0x110,0x120,2));assert run_until_break(g,symbols['field_runtime_active'],300)
begin('ps4_field_system',(0,0));g.run(60)
for _ in range(4):press('DOWN',after=12)
press('A',after=35)
for _ in range(9):press('DOWN',after=12)
press('A',after=35)
for _ in range(3):press('DOWN',after=12)
press('A',after=35)
for _ in range(4):press('RIGHT',after=12)
for _ in range(14):press('DOWN',after=12)
L.mg_break(symbols['ps4_debug_queue_story_event']);g.press('A',3,60)
assert L.mg_hit() and g.reg(0)==0x4e,'actual STORY selector did not queue DF2 event'
assert run_until_break(g,0x08800000,60),'SYSTEM did not close after story warp'
assert g.rd16(symbols['ps4_debug_event_pending'])==0x4f and read16(g.mem(md+0xec28,2),0)==0x19f
report['actual_SYSTEM_DEBUG_STORY_DF2_selection']=True
report['all_scenes_prerequisites_full_playthrough_physical_rendering_verified']=False
(O/(prefix+'_MGBA_BOSS_STORY_DEBUG.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS actual SYSTEM > DEBUG > STORY selection and warp queue',flush=True)
