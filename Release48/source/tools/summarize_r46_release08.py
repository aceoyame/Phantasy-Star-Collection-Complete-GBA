#!/usr/bin/env python3
"""Consolidate Release 08 evidence against the complete accepted Release 07."""
from pathlib import Path
import argparse,json,re,zipfile,zlib
from psc_rom_space import REGIONS,available_padding,check_reclaimed_padding
R=Path(__file__).resolve().parents[2];S=R/'source';O=R/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--baseline-zip',type=Path,required=True);a=ap.parse_args()
def crc(b):return f'{zlib.crc32(b):08x}'
def read(n):return json.loads((O/n).read_text())
def write(n,d):(O/n).write_text(json.dumps(d,indent=2)+'\n')
rom=(R/'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes()
module=(S/'build-psc-module/ps4_psc_module.raw.gba').read_bytes();donor=(R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()
assert len(rom)==0x2000000 and len(donor)==0x800000
assert rom[:0x800000]==donor and rom[0x800000:0x800000+len(module)]==module
assert rom[0x800000+len(module):]==b'\xff'*(0x1800000-len(module))
rom_id={'rom_bytes':len(rom),'rom_crc32':crc(rom)}
assert read('R46_RELEASE08_MGBA_DEBUG.rom_crc.json')==rom_id
mp=(S/'build-psc-module/ps4_psc_module.map').read_text()
def symbol(n):return int(re.search(r'^\s*([\da-fA-F]+)\s+[^\n]*\b'+n+r'\s*=',mp,re.M)[1],16)
iw,ew=symbol('__iwram_alloc_end'),symbol('__bss_end');assert iw==0x3006c98 and ew==0x2029fc6
def map_symbols(text):
    out={}
    for line in text.splitlines():
        p=line.split()
        if len(p)>=5:
            try:out[p[-1]]=int(p[0],16)
            except ValueError:pass
    return out
symbols=map_symbols(mp)
assert symbols['ps4_psc_panel_bank']>=0x08800000 and symbols['ps4_source_enemy_bank']>=0x08800000
files={str(p.relative_to(S)):p for p in (S/'gba').rglob('*') if p.is_file() and p.suffix not in ('.o','.su','.pyc','.so') and '__pycache__' not in p.parts and not p.name.endswith('.o.tmp')}
def function(text,name):
    m=re.search(r'^static [^\n]*\b'+name+r'\([^\n]*\)\{',text,re.M);assert m,name
    i=m.end();depth=1
    while depth:
        depth+=(text[i]=='{')-(text[i]=='}');i+=1
    return text[m.start():i]
with zipfile.ZipFile(a.baseline_zip) as z:
    old_names={n.removeprefix('R46/source/') for n in z.namelist() if n.startswith('R46/source/gba/') and not n.endswith('/')}
    assert set(files)==old_names
    changed=[]
    for f in sorted(old_names):
        old=z.read('R46/source/'+f);new=files[f].read_bytes()
        if old!=new:changed.append({'file':f,'before_bytes':len(old),'after_bytes':len(new),'before_crc32':crc(old),'after_crc32':crc(new)})
    assert {c['file'] for c in changed}=={'gba/link_psc.ld','gba/precomp_assets.bin','gba/precomp_assets.h','gba/psc_field_predecode.bin','gba/psc_field_predecode.c','gba/psc_field_predecode.h','gba/title_runtime_v06.c'}
    protected=json.loads(z.read('R46/build_artifacts/R45_PRESERVATION.json'))['unchanged_audio_files']
    assert len(protected)==27 and all((S/f).read_bytes()==z.read('R46/source/'+f) for f in protected)
    prior_donor=z.read('R46/donors/PSC_R15_shell_8MiB.gba')
    assert donor==prior_donor,'Release 07 already has empty verified padding; all original PSC bytes must stay identical'
    regions=check_reclaimed_padding(prior_donor,donor)
    prior=json.loads(z.read('R46/build_artifacts/R46_RELEASE07_VALIDATION.json'))
    assert prior['ROM_crc32']=='979c0c64'
    poses=(S/'gba/r31_field_poses.bin').read_bytes();assert poses==z.read('R46/source/gba/r31_field_poses.bin')
    assert (R/'AUDIO_BANK_PROFILES.json').read_bytes()==z.read('R46/AUDIO_BANK_PROFILES.json')
    profiles=json.loads((R/'AUDIO_BANK_PROFILES.json').read_text())['profiles']
    for profile in profiles.values():
        for f in profile['files'].values():
            b=(R/f['file']).read_bytes();assert len(b)==f['bytes'] and crc(b)==f['crc32'] and b==z.read('R46/'+f['file'])
    rollback='rollback/R46_RELEASE05_TRANSPORT/R46/source/gba/ps4_audio_stream.c'
    assert (R/rollback).read_bytes()==z.read('R46/'+rollback)
    oldruntime=z.read('R46/source/gba/title_runtime_v06.c').decode();runtime=(S/'gba/title_runtime_v06.c').read_text()
    math_names=['battle_execute_cmd','battle_calc_chance','battle_source_damage','battle_apply_enemy_effect','battle_consume_action_resource','battle_action_has_resource','nemesis_fast_fallback']
    assert all(function(runtime,n)==function(oldruntime,n) for n in math_names)
    assert (S/'gba/precomp_assets.bin').read_bytes()==z.read('R46/source/gba/precomp_assets.bin')[116736:]
    assert (S/'gba/psc_field_fastassets.bin').read_bytes()==z.read('R46/source/gba/psc_field_fastassets.bin')
padding=available_padding(donor);tail=0x1800000-len(module);gain=prior['build']['module_bytes']-len(module)
assert padding==1220500 and gain>1000000
space={'ROM_bytes':len(rom),'PSIV_module_bytes_before':prior['build']['module_bytes'],'PSIV_module_bytes':len(module),
       'newly_reclaimed_module_bytes':gain,'PSIV_tail_bytes_before':prior['build']['ROM_tail_headroom_bytes'],
       'PSIV_tail_bytes':tail,'PSIV_tail_file_start':hex(0x800000+len(module)),
       'PSC_original_game_reserved_padding_bytes':padding,'PSC_padding_already_empty_in_Release07':True,
       'newly_reclaimed_PSC_padding_bytes':0,'total_available_bytes':tail+padding,
       'ranges_are_separate_not_one_contiguous_allocation':True,'PSC_regions':regions,
       'unused_visual_screens_removed_bytes':116736,'lossless_field_bank_reduction_bytes':963072,
       'added_code_and_alignment_bytes':1079808-gain,'future_original_game_additions_implemented':False}
write('R46_RELEASE08_ROM_SPACE.json',space)
write('R46_RELEASE08_PSC_PADDING.json',{'regions':regions,'all_8MiB_PSC_bytes_identical_to_Release07':True,
      'PSC_reserved_bytes':padding,'newly_reclaimed_PSC_bytes':0,'already_empty_in_Release07':True,
      'source_proof':'Pinned build_psc_four_game.py retail trailing-zero intervals; current map defines both banks in the PSIV module. Stale linker aliases removed.'})
preservation={'baseline':'Release 07 / 979c0c64','GBA_production_files':len(files),'changed_existing_GBA_files':changed,
              'other_existing_GBA_files_byte_identical':len(files)-len(changed),'protected_audio_files_byte_identical':27,
              'all_8MiB_PSC_donor_launcher_original_games_save_patches_byte_identical':True,
              'pose_bank_bytes_byte_identical':len(poses),'all_PD_and_other_unchanged_GBA_assets_byte_identical':True,
              'per_frame_field_fastassets_byte_identical':True,'retained_visual_bytes_identical_after_116736_byte_shift':True,
              'math_RNG_resource_and_fallback_functions_byte_identical':math_names,
              'rollback_profiles':{k:len(v['files']) for k,v in profiles.items()},'exact_Release05_transport_byte_identical':True,
              'IWRAM_and_EWRAM_allocation_endpoints_unchanged':True,'ability_tracks_use_448_of_existing_640_scratch_bytes':True}
write('R46_RELEASE08_PRODUCTION_GUARD.json',preservation)
pack=read('R46_RELEASE08_FIELD_PACKING.json');assert pack['ROM_crc32']==crc(rom) and pack['source_records']==969 and pack['native_ARM_copy_and_capacity_cases']==5817
debuff=read('R46_RELEASE08_MGBA_DEBUFF_SKILLS.json');assert debuff['rom']==rom_id and len(debuff['cases'])==22
tech=read('R46_RELEASE08_MGBA_STATUS_CONTROLLERS.json');assert tech['rom']==rom_id and len(tech['cases'])==35
stats=read('R46_RELEASE08_MGBA_STATS.json');assert stats['matching_ROM_state_crc_verified'] and len(stats['cases'])==34
field=read('R46_RELEASE08_MGBA_FIELD_AUDIO.json');assert len(field['field'])==34 and all(c['audio_delta']['underruns']==0 for c in field['field'])
assert field['recovery']['recovered_300_frames']['underruns']==0 and field['recovery']['last_second_pcm_rms']>1
psc=read('R46_RELEASE08_MGBA_PSC_SPACE.json');assert psc['ROM_crc32']==crc(rom) and len(psc['routes'])==4
camp=read('R46_RELEASE08_MGBA_CAMP_REDRAW.json');assert camp['rom']==rom_id and camp['baseline_crc32']=='979c0c64' and camp['full_framebuffers_palettes_and_OAM_identical']
boss=read('R46_RELEASE08_MGBA_BOSS_STORY_DEBUG.json');assert boss['rom']==rom_id and len(boss['caption_cases'])==153 and len(boss['DF2'])==2
assert all(c['palette_matches_original_source'] and c['idle_updates_guarded']==48 for c in boss['DF2'])
assert len(boss['queue_cases'])==160 and len(boss['map_handoffs'])==8 and boss['actual_SYSTEM_DEBUG_STORY_DF2_selection']
seams=read('R46_RELEASE08_FIFO_SEAMS.json');assert len(seams)==180 and all(c['before']['pending_mismatches']==c['after']['pending_mismatches']==0 for c in seams)
report={'release':'R46 release 08','ROM_crc32':crc(rom),'build':{'ROM_bytes':len(rom),'module_bytes':len(module),
        'ROM_tail_headroom_bytes':tail,'PSC_reserved_padding_bytes':padding,'total_available_ROM_bytes':tail+padding,
        'iwram_end':hex(iw),'iwram_margin_bytes':0x3006d00-iw,'ewram_bss_end':hex(ew),'ewram_margin_bytes':0x202a000-ew},
        'ROM_space':space,'preservation':preservation,'field_packing':pack,
        'Telele_Shadow':{'native_cases':22,'source_success_only_12_wave_fanout_and_conditional_cues':True,
        'scope':'Actual ARM controller fixtures, whole actors/saved/RNG/charges and every update OBJ art/anchors/palette/cues. Full source fighter-pose/68000 runner parity and ordinary-save playthrough remain open.',
        'report':'R46_RELEASE08_MGBA_DEBUFF_SKILLS.json'},
        'regressions':{'offensive_TECH_controller_cases':35,'Vision_stat_cases':34,'field_stationary_movement_cases':34,
        'field_starvation_recovery':field['recovery'],'boss_target_captions':153,'DF2_routes':boss['DF2'],
        'story_queue_choices':160,'story_loader_handoffs':8,'actual_debug_story_menu_selection':True,
        'PSC_startup_AB_routes':psc['routes'],'PSC_full_interactive_playthrough_verified':False},
        'CAMP_AB':{'identical_full_framebuffers_palettes_OAM':True,'cases':camp['cases'],
        'load_tradeoff':'Piata reaches field-runtime-active in 27 display frames versus 19 in Release 07. Lossless load-time decoding adds eight frames; cursor cost remains within 1.1% of baseline. Other load timings/hardware remain open.'},
        'FIFO':{'native_title_commits':180,'before_after_mismatches':0,'max_commit_CPU_cycles':max(c['cycles'] for c in seams)},
        'current_priorities':['combat','quests/progression','field/interface','ending/saves','PD improvements last'],
        'accepted_audio_and_all_wave_rollback_profiles_preserved':True,'PD_changes_implemented':False,
        'historical_evidence_not_current_validation':['Older full music/SFX/chip listening coverage','Earlier Guild/story/ending/normal-save evidence','Earlier PD controller/performance traces'],
        'finishability_certified':False,'physical_hardware_verified':False}
write('R46_RELEASE08_VALIDATION.json',report)
print(json.dumps({'ROM_crc32':crc(rom),'build':report['build'],'new_ROM_bytes_recovered':gain,
      'changed_GBA_files':[c['file'] for c in changed],'protected_audio_files':27,
      'new_debuff_cases':22,'field_packing_ARM_cases':5817,'load_tradeoff_display_frames':8},indent=2))
