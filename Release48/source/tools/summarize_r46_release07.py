#!/usr/bin/env python3
"""Summarize Release 07 evidence and guard the accepted Release 06 baseline."""
import argparse,json,re,zipfile,zlib
from pathlib import Path

R=Path(__file__).resolve().parents[2];S=R/'source';O=R/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--baseline-zip',type=Path,required=True);a=ap.parse_args()
def crc(b):return f'{zlib.crc32(b):08x}'
def read(n):return json.loads((O/n).read_text())
def write(n,v):(O/n).write_text(json.dumps(v,indent=2)+'\n')
rom=(R/'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes()
module=(S/'build-psc-module/ps4_psc_module.raw.gba').read_bytes()
donor=(R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()
assert len(rom)==0x2000000 and len(donor)==0x800000
assert rom[:0x800000]==donor and rom[0x800000:0x800000+len(module)]==module
assert rom[0x800000+len(module):]==b'\xff'*(0x1800000-len(module))
rom_id={'rom_bytes':len(rom),'rom_crc32':crc(rom)}
assert read('R46_RELEASE07_MGBA_DEBUG.rom_crc.json')==rom_id
mp=(S/'build-psc-module/ps4_psc_module.map').read_text()
def symbol(n):return int(re.search(r'^\s*([\da-fA-F]+)\s+[^\n]*\b'+n+r'\s*=',mp,re.M)[1],16)
iw,ew=symbol('__iwram_alloc_end'),symbol('__bss_end')
assert iw<=0x3006d00 and ew<=0x202a000
files={str(p.relative_to(S)):p for p in (S/'gba').rglob('*') if p.is_file() and p.suffix not in ('.o','.su','.pyc','.so') and '__pycache__' not in p.parts and not p.name.endswith('.o.tmp')}
with zipfile.ZipFile(a.baseline_zip) as z:
    old_names={n.removeprefix('R46/source/') for n in z.namelist() if n.startswith('R46/source/gba/') and not n.endswith('/')}
    assert old_names<=set(files) and set(files)-old_names=={'gba/r46_debug_event_locations.h'}
    changed=[]
    for f in sorted(old_names):
        old=z.read('R46/source/'+f);new=files[f].read_bytes()
        if old!=new:changed.append({'file':f,'before_bytes':len(old),'after_bytes':len(new),'before_crc32':crc(old),'after_crc32':crc(new)})
    assert [c['file'] for c in changed]==['gba/precomp_assets.bin','gba/title_runtime_v06.c']
    old=z.read('R46/source/gba/precomp_assets.bin');new=files['gba/precomp_assets.bin'].read_bytes()
    off=read('R46_RELEASE07_DF2_PALETTE.json')['asset_offset']
    assert len(old)==len(new) and old[:off]==new[:off] and old[off+32:]==new[off+32:]
    palette_changed_bytes=sum(x!=y for x,y in zip(old[off:off+32],new[off:off+32]))
    assert palette_changed_bytes==19
    protected=json.loads(z.read('R46/build_artifacts/R45_PRESERVATION.json'))['unchanged_audio_files']
    assert len(protected)==27 and all((S/f).read_bytes()==z.read('R46/source/'+f) for f in protected)
    assert donor==z.read('R46/donors/PSC_R15_shell_8MiB.gba')
    assert (S/'gba/r31_field_poses.bin').read_bytes()==z.read('R46/source/gba/r31_field_poses.bin')
    assert (R/'AUDIO_BANK_PROFILES.json').read_bytes()==z.read('R46/AUDIO_BANK_PROFILES.json')
    profiles=json.loads((R/'AUDIO_BANK_PROFILES.json').read_text())['profiles']
    for profile in profiles.values():
        for f in profile['files'].values():
            b=(R/f['file']).read_bytes()
            assert len(b)==f['bytes'] and crc(b)==f['crc32'] and b==z.read('R46/'+f['file'])
    rollback='rollback/R46_RELEASE05_TRANSPORT/R46/source/gba/ps4_audio_stream.c'
    assert (R/rollback).read_bytes()==z.read('R46/'+rollback)
    generator='tools/generate_precompiled_visuals.py'
    assert (S/generator).read_bytes()!=z.read('R46/source/'+generator)

controllers=read('R46_RELEASE07_MGBA_STATUS_CONTROLLERS.json')
assert controllers['rom']==rom_id and controllers['native_controller_cases']==len(controllers['cases'])==35
assert all(c['TP_once_saved_RNG_actor_guards'] for c in controllers['cases'])
status=read('R46_RELEASE07_MGBA_STATUS_EFFECTS.json');stats=read('R46_RELEASE07_MGBA_STATS.json')
assert status['native_cases']==len(status['cases'])==138 and status['matching_ROM_state_crc_verified']
assert len(stats['cases'])==34 and stats['matching_ROM_state_crc_verified']
camp=read('R46_RELEASE07_MGBA_CAMP_REDRAW.json')
assert camp['rom']==rom_id and camp['baseline_crc32']=='cb75252c' and camp['full_framebuffers_palettes_and_OAM_identical']
camp_perf=[]
for count in (1,5):
    old=next(c for c in camp['cases'] if c['ROM']=='baseline' and c['party_count']==count)
    new=next(c for c in camp['cases'] if c['ROM']=='release07' and c['party_count']==count)
    assert old['cursor_moves']==new['cursor_moves']==14
    assert old['confirm_SKILL_cancel_wrap_saved_palettes_OAM_checked'] and new['confirm_SKILL_cancel_wrap_saved_palettes_OAM_checked']
    assert new['median_redraw_cycles']<old['median_redraw_cycles']
    camp_perf.append({'party_count':count,'baseline_median_CPU_cycles':old['median_redraw_cycles'],'release07_median_CPU_cycles':new['median_redraw_cycles'],'redraw_cost_reduction_percent':100*(1-new['median_redraw_cycles']/old['median_redraw_cycles'])})
boss=read('R46_RELEASE07_MGBA_BOSS_STORY_DEBUG.json')
assert boss['rom']==rom_id and len(boss['caption_cases'])==153
assert all(c['native_target_glyphs_match'] for c in boss['caption_cases'])
labels=[c for c in boss['caption_cases'] if c['readable_placeholder_override']]
assert [c['enemy_id'] for c in labels]==list(range(0x81,0x88))
assert len(boss['DF2'])==2 and all(c['palette_matches_original_source'] and c['idle_updates_guarded']==48 and len(c['colored_body_indices'])>=4 for c in boss['DF2'])
assert len(boss['queue_cases'])==160 and sum(c['handled'] for c in boss['queue_cases'])==150
assert all(c['resource_flag_and_pending_guards'] for c in boss['queue_cases'])
assert len(boss['map_handoffs'])==8 and all(c['loaded_descriptor_geometry_and_all_16_pointers_match'] and c['pending_consumed_once'] for c in boss['map_handoffs'])
assert boss['actual_SYSTEM_DEBUG_STORY_DF2_selection']
locations=read('R46_RELEASE07_DEBUG_EVENT_LOCATIONS.json')
assert locations['located_events']==len(locations['selected'])==150 and len(locations['unlocated_events'])==10
seams=read('R46_RELEASE07_FIFO_SEAMS.json')
assert len(seams)==180 and all(c[k]['pending_bytes'] and c[k]['pending_mismatches']==0 for c in seams for k in ('before','after'))
preservation={'baseline':'R46 release 06 / cb75252c','baseline_GBA_production_files':len(old_names),'current_GBA_production_files':len(files),'changed_existing_GBA_files':changed,'added_GBA_files':['gba/r46_debug_event_locations.h'],'other_existing_GBA_files_byte_identical':len(old_names)-len(changed),'changed_asset_entry':{'offset':off,'entry_bytes':32,'changed_bytes':palette_changed_bytes,'all_other_precompiled_visual_bytes_identical':True},'changed_existing_generator':generator,'new_generator':'tools/generate_r46_debug_event_locations.py','protected_audio_files_byte_identical':len(protected),'PD_renderer_controller_assets_byte_identical':True,'donor_and_pose_bank_byte_identical':True,'rollback_profiles':{k:len(v['files']) for k,v in profiles.items()},'exact_release05_transport_retained_byte_identical':True}
write('R46_RELEASE07_PRODUCTION_GUARD.json',preservation)
report={'release':'R46 release 07','ROM_crc32':crc(rom),'build':{'ROM_bytes':len(rom),'module_bytes':len(module),'ROM_tail_headroom_bytes':0x1800000-len(module),'iwram_end':hex(iw),'iwram_margin_bytes':0x3006d00-iw,'ewram_bss_end':hex(ew),'ewram_margin_bytes':0x202a000-ew},'preservation':preservation,'status_controllers':{'native_cases':35,'techniques':['Vol','Savol','Gelun','Doran','Seals','Rimit'],'scope':'Source success/miss masks, per-update OBJ art/placement/palette/cues, deferred HP/sleep callbacks, whole actor/RNG/resource guards; initialized native fixtures, not ordinary-save/physical timing certification','report':'R46_RELEASE07_MGBA_STATUS_CONTROLLERS.json'},'regressions':{'status_cases':138,'Vision_stat_cases':34,'matching_current_ROM_state_CRC_verified':True},'CAMP':{'performance':camp_perf,'identical_native_framebuffers_palettes_OAM':True,'scope':'14 up/down/wrap movements per one/five-member party on each ROM, confirm SKILL/cancel, saved resource guards; CPU redraw slice excludes wait/input and does not certify first-open/device latency'},'boss_names':{'native_target_captions_checked':153,'placeholder_names_corrected':labels,'scope':'All native target glyphs checked; ordinary boss encounters and every caption context not fully replayed'},'DF2':{'source_palette':'loc_317DC','native_checks':boss['DF2'],'live_screenshots_inspected':True,'opening_fade_controller_verified':False},'story_debug':{'choices_checked':160,'authored_destinations':150,'unavailable':locations['unlocated_events'],'native_map_handoffs':boss['map_handoffs'],'actual_SYSTEM_DEBUG_STORY_DF2_selection':True,'scope':locations['scope']},'FIFO_seams':{'native_title_commits':len(seams),'queue_mismatches_before':0,'queue_mismatches_after':0,'queued_bytes_checked':sum(len(c[k]['pending_bytes']) for c in seams for k in ('before','after')),'max_commit_CPU_cycles':max(c['cycles'] for c in seams),'scope':'Focused actual upstream mGBA cold-title FIFO/DMA continuity check; accepted Release 06 audio sources unchanged; broader audio/device tests remain historical'},'historical_evidence_not_rerun':['Release 06 full 3600-frame title/audio deadline/SFX/field recovery checks','Release 05 chip/wave/music coverage','CP29 PD renderer/controller profiles','Earlier quest/ending/normal-save checks'],'current_priorities':['combat presentation and behavior','quests/progression','field/interface','ending/saves','PD improvements last'],'audio_instrumentation_accepted_by_user':True,'instrumentation_changed':False,'PD_renderer_controller_changed_or_tested':False,'finishability_certified':False,'physical_hardware_verified':False}
write('R46_RELEASE07_VALIDATION.json',report)
print(json.dumps({'ROM_crc32':crc(rom),'build':report['build'],'changed_GBA_files':[c['file'] for c in changed],'unchanged_audio_files':len(protected),'CAMP':camp_perf,'native_status_cases':35+138+34,'story_destinations':150,'FIFO_commits':180},indent=2))
