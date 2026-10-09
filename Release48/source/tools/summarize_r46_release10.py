#!/usr/bin/env python3
"""Current-ROM release evidence and exact Release09 preservation guards."""
from pathlib import Path
import argparse,json,re,zipfile,zlib
R=Path(__file__).resolve().parents[2];S=R/'source';O=R/'build_artifacts'
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path,required=True);a=p.parse_args()
def crc(b):return f'{zlib.crc32(b):08x}'
def read(n):return json.loads((O/n).read_text())
def write(n,v):(O/n).write_text(json.dumps(v,indent=2)+'\n')
def function(t,n):
 m=re.search(r'^static [^{}\n]*\b'+n+r'\([^{}\n]*\)\{',t,re.M);assert m,n
 i=m.end();depth=1
 while depth:depth+=(t[i]=='{')-(t[i]=='}');i+=1
 return t[m.start():i]
rom=(R/'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes();module=(S/'build-psc-module/ps4_psc_module.raw.gba').read_bytes();donor=(R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()
assert len(rom)==0x2000000 and len(donor)==0x800000 and rom[:0x800000]==donor and rom[0x800000:0x800000+len(module)]==module and rom[0x800000+len(module):]==b'\xff'*(0x1800000-len(module))
rom_id={'rom_bytes':len(rom),'rom_crc32':crc(rom)};assert read('R46_RELEASE10_MGBA_DEBUG.rom_crc.json')==rom_id
mp=(S/'build-psc-module/ps4_psc_module.map').read_text()
def sym(n):return int(re.search(r'^\s*([\da-fA-F]+)\s+[^\n]*\b'+n+r'\s*=',mp,re.M)[1],16)
iw,ew=sym('__iwram_alloc_end'),sym('__bss_end');assert (iw,ew)==(0x3006c78,0x2029fc6)
with zipfile.ZipFile(a.baseline_zip) as z:
 oldfiles={n.removeprefix('R46/source/') for n in z.namelist() if n.startswith('R46/source/gba/') and not n.endswith('/')}
 files={str(p.relative_to(S)):p for p in (S/'gba').rglob('*') if p.is_file() and p.suffix not in ('.o','.su','.pyc','.so') and '__pycache__' not in p.parts and not p.name.endswith('.o.tmp')}
 assert set(files)==oldfiles
 changed=[f for f in sorted(files) if files[f].read_bytes()!=z.read('R46/source/'+f)];assert changed==['gba/title_runtime_v06.c']
 protected=json.loads(z.read('R46/build_artifacts/R45_PRESERVATION.json'))['unchanged_audio_files'];assert len(protected)==27 and all((S/f).read_bytes()==z.read('R46/source/'+f) for f in protected)
 assert donor==z.read('R46/donors/PSC_R15_shell_8MiB.gba')
 assert (R/'AUDIO_BANK_PROFILES.json').read_bytes()==z.read('R46/AUDIO_BANK_PROFILES.json')
 profiles=json.loads((R/'AUDIO_BANK_PROFILES.json').read_text())['profiles'];wavefiles=[]
 for profile in profiles.values():
  for f in profile['files'].values():
   b=(R/f['file']).read_bytes();assert len(b)==f['bytes'] and crc(b)==f['crc32'] and b==z.read('R46/'+f['file']);wavefiles.append(f['file'])
 rollback='rollback/R46_RELEASE05_TRANSPORT/R46/source/gba/ps4_audio_stream.c';assert (R/rollback).read_bytes()==z.read('R46/'+rollback)
 old=z.read('R46/source/gba/title_runtime_v06.c').decode();new=(S/'gba/title_runtime_v06.c').read_text()
 guards=['battle_execute_cmd','battle_calc_chance','battle_source_damage','battle_apply_enemy_effect','battle_consume_action_resource','battle_action_has_resource','battle_ui_stage_generic_anim','battle_party_source_debuff_skill','battle_action_idle_after_wait','psc_field_expand','battle_choose_main_option','battle_command_for_actor','battle_choose_ability','battle_choose_target','battle_choose_ally']
 # Decoder name is build-specific; all code outside the declared UI/controller
 # edit regions must be byte-identical, a stronger guard than individual calls.
 def normalize(t):
  for n in ['battle_ui_pd_menu_pause','battle_ui_idle_vblank','battle_ui_wait_edge','battle_party_source_illusion_skill','battle_party_source_skill']:
   if re.search(r'^static [^\n]*\b'+n+r'\(',t,re.M):t=t.replace(function(t,n),'')
  t=re.sub(r'/\* PD bitmap composition.*?\*/','',t,flags=re.S)
  t=re.sub(r'/\* BattleObj_Illusion / IllusionAnim_Init/Main:.*?\*/','',t,flags=re.S)
  return '\n'.join(line for line in t.splitlines() if line.strip())
 assert normalize(old).strip()==normalize(new).strip(),'unexpected runtime edits'
 guards.remove('psc_field_expand')
 assert all(function(old,n)==function(new,n) for n in guards)
 prior=json.loads(z.read('R46/build_artifacts/R46_RELEASE09_VALIDATION.json'))
preservation={'baseline':'Release09 / 79384880','GBA_production_files':len(files),'changed_existing_GBA_files':changed,'other_GBA_files_byte_identical':len(files)-len(changed),'protected_audio_files_byte_identical':len(protected),'wave_profile_files_byte_identical':len(wavefiles),'complete_8MiB_PSC_donor_byte_identical':True,'all_assets_and_ROLLBACK_transport_byte_identical':True,'unchanged_runtime_functions':guards,'all_other_runtime_nonblank_lines_byte_identical':True,'no_new_persistent_RAM':True}
space={'ROM_bytes':len(rom),'PSIV_module_bytes':len(module),'PSIV_tail_bytes':0x1800000-len(module),'PSIV_tail_file_start':hex(0x800000+len(module)),'PSC_original_game_reserved_padding_bytes':1220500,'total_available_bytes':0x1800000-len(module)+1220500,'newly_recovered_vs_Release07_bytes':prior['build']['newly_recovered_vs_Release07_bytes']-(len(module)-prior['build']['PSIV_module_bytes']),'Release10_code_growth_bytes':len(module)-prior['build']['PSIV_module_bytes'],'separate_ranges':True,'PSC_padding_already_empty_in_Release07':True,'iwram_end':hex(iw),'iwram_margin_bytes':0x3006d00-iw,'ewram_bss_end':hex(ew),'ewram_margin_bytes':0x202a000-ew,'future_original_game_additions_implemented':False}
menus=read('R46_RELEASE10_MGBA_PD_MENUS.json');illusion=read('R46_RELEASE10_MGBA_ILLUSION.json');pd=read('R46_RELEASE10_PD_CONTROLLER_AB.json');ret=read('R46_RELEASE10_MGBA_PD_RETURN.json')
assert menus['rom']==illusion['rom']==pd['current_ROM']==rom_id
assert len(illusion['cases'])==8 and pd['source_scene_comparisons']==1306
assert len(menus['polling_AB'])==6 and all(v['display_frames']==120 and v['audio_delta'][2]==0 for v in menus['polling_AB'] if v['ROM']=='current')
assert ret['PD1_native_debug_battle']['audio_delta']['underruns']==ret['Piata_native_field_continuation']['audio_delta']['underruns']==0
report={'release':'R46 release 10','ROM_crc32':crc(rom),'build':space,'preservation':preservation,'PD_menu_latency':menus,'Illusion':illusion,'PD_action_AB':pd,'genuine_debug_PD_RUN_Piata':ret,'prior_evidence_retained_not_rerun':['Release09 loading/field packing/body-row/pixel tests','Release08 DF2/captions/story-warp/Telele/Shadow/quest/save history'],'finishability_certified':False,'physical_hardware_verified':False,'known_issues':'RELEASE_NOTES_R46_RELEASE_10.md'}
write('R46_RELEASE10_PRODUCTION_GUARD.json',preservation);write('R46_RELEASE10_ROM_SPACE.json',space);write('R46_RELEASE10_VALIDATION.json',report)
print(json.dumps({'ROM_crc32':crc(rom),'space':space,'changed_GBA':changed,'Illusion_cases':8,'PD_authored_scenes':1306},indent=2))
