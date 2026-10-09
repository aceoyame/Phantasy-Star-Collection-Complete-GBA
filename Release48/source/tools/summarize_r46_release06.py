#!/usr/bin/env python3
"""Validate the final transport release against its accepted Release 05 baseline."""
import argparse,json,re,zipfile,zlib
from pathlib import Path
R=Path(__file__).resolve().parents[2];O=R/'build_artifacts'
p=argparse.ArgumentParser();p.add_argument('--baseline-zip',type=Path,required=True);a=p.parse_args()
def crc(b):return f'{zlib.crc32(b):08x}'
def read(n):return json.loads((O/n).read_text())
def write(n,q):(O/n).write_text(json.dumps(q,indent=2)+'\n')
files=sorted(p for p in (R/'source/gba').rglob('*') if p.is_file() and p.suffix not in ('.o','.su','.pyc') and '__pycache__' not in p.parts and not p.name.endswith('.o.tmp'))
with zipfile.ZipFile(a.baseline_zip) as z:
 names={n.removeprefix('R46/source/') for n in z.namelist() if n.startswith('R46/source/gba/') and not n.endswith('/')}
 assert {str(p.relative_to(R/'source')) for p in files}==names
 changed=[]
 for p in files:
  rel=str(p.relative_to(R/'source'));old=z.read('R46/source/'+rel);new=p.read_bytes()
  if old!=new:changed.append(dict(file=rel,before_bytes=len(old),after_bytes=len(new),before_crc32=crc(old),after_crc32=crc(new)))
 assert [x['file'] for x in changed]==['gba/ps4_audio_stream.c']
 protected=json.loads(z.read('R46/build_artifacts/R45_PRESERVATION.json'))['unchanged_audio_files']
 assert 'gba/ps4_audio_stream.c' in protected
 assert all((R/'source'/f).read_bytes()==z.read('R46/source/'+f) for f in protected if f!='gba/ps4_audio_stream.c')
 assert (R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()==z.read('R46/donors/PSC_R15_shell_8MiB.gba')
 assert (R/'source/gba/r31_field_poses.bin').read_bytes()==z.read('R46/source/gba/r31_field_poses.bin')
 rollback=R/'rollback/R46_RELEASE05_TRANSPORT/R46/source/gba/ps4_audio_stream.c'
 assert rollback.read_bytes()==z.read('R46/source/gba/ps4_audio_stream.c')
profiles=json.loads((R/'AUDIO_BANK_PROFILES.json').read_text())['profiles']
for profile in profiles.values():
 for q in profile['files'].values():
  b=(R/q['file']).read_bytes();assert len(b)==q['bytes'] and crc(b)==q['crc32']
rom=(R/'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes();module=(R/'source/build-psc-module/ps4_psc_module.raw.gba').read_bytes()
assert len(rom)==0x2000000 and rom[0x800000:0x800000+len(module)]==module
assert rom[:0x800000]==(R/'donors/PSC_R15_shell_8MiB.gba').read_bytes()
assert rom[0x800000+len(module):]==b'\xff'*(0x1800000-len(module))
mp=(R/'source/build-psc-module/ps4_psc_module.map').read_text()
def symbol(n):return int(re.search(r'^\s*([\da-fA-F]+)\s+[^\n]*\b'+n+r'\s*=',mp,re.M)[1],16)
iw,ew=symbol('__iwram_alloc_end'),symbol('__bss_end');assert iw<=0x3006d00 and ew<=0x202a000
seams=read('R46_RELEASE06_FIFO_SEAMS.json');assert len(seams)==3600
assert all(x[k]['pending_bytes'] and x[k]['pending_mismatches']==0 for x in seams for k in ('before','after'))
base=read('R46_RELEASE05_FIFO_SEAMS.json');assert len(base)==600
seam_report=dict(native_title_commits=len(seams),queue_mismatches_before=0,queue_mismatches_after=0,queued_bytes_checked=sum(len(x[k]['pending_bytes']) for x in seams for k in ('before','after')),max_commit_CPU_cycles=max(x['cycles'] for x in seams),baseline_600_commits_after_bad=sum(bool(x['after']['pending_mismatches']) for x in base),baseline_600_commits_after_mismatched_bytes=sum(x['after']['pending_mismatches'] for x in base),scope='actual upstream mGBA FIFO and live DMA at cold native-title block commits; not physical hardware listening')
title=read('R46_RELEASE06_TITLE_AUDIO.json');assert title['ROM_crc32']==crc(rom) and title['frames']==3600
last=title['rows'][-1];assert last['underruns']==0 and last['music_sequence']==last['productions']
state=read('R46_RELEASE06_MGBA_DEBUG.rom_crc.json');assert state['rom_crc32']==crc(rom)
host=read('R46_RELEASE06_SFX_OWNERSHIP.json');native=read('R46_RELEASE06_MGBA_SFX_OWNERSHIP.json');assert host['cases']==native['cases']==402 and native['all_match']
dead=read('R46_RELEASE06_MGBA_MUSIC_DEADLINES.json');assert len(dead['songs'])==52
assert all(x['audio_delta']['underruns']==x['audio_delta']['late_commits']==0 and abs(x['audio_delta']['productions']-300)<=1 for x in dead['songs'])
field=read('R46_RELEASE06_MGBA_FIELD_AUDIO.json');assert len(field['field'])==34 and all(x['audio_delta']['underruns']==0 for x in field['field'])
assert field['recovery']['starved']['underruns']>0 and field['recovery']['recovered_300_frames']['underruns']==0
preservation=dict(production_files=len(files),changed=changed,other_production_files_byte_identical=len(files)-1,protected_audio_files_unchanged=len(protected)-1,protected_audio_files_changed=1,waves_trims_player_SFX_gameplay_PD_unchanged=True,donor_and_pose_bank_unchanged=True,rollback_profiles={k:len(v['files']) for k,v in profiles.items()},exact_release05_transport_retained=True)
write('R46_RELEASE06_PRODUCTION_GUARD.json',preservation)
report=dict(release='R46 release 06',ROM_crc32=crc(rom),build=dict(ROM_bytes=len(rom),module_bytes=len(module),ROM_tail_headroom_bytes=0x1800000-len(module),iwram_end=hex(iw),iwram_margin_bytes=0x3006d00-iw,ewram_bss_end=hex(ew),ewram_margin_bytes=0x202a000-ew),preservation=preservation,FIFO_seams=seam_report,native_title=dict(last=last,PCM_RMS=title['PCM_RMS'],baseline_release05_late_commits=3573,scope=title['scope']),SFX_host=host,SFX_native=native,music_deadlines=dead,field_audio=field,historical_release05_evidence_not_rerun=['52-song ASan/FM coverage and gain checks','52-song 13000-frame note traces','1280 host/native PSG checks','7-song native/host PCM parity'],hardware_listening_confirmed=False,finishability_certified=False,PD_changed_or_tested=False)
write('R46_RELEASE06_VALIDATION.json',report)
print(json.dumps(dict(ROM_crc32=crc(rom),build=report['build'],FIFO=seam_report,title=last,field_late=sum(x['audio_delta']['late_commits'] for x in field['field']),music_max_CPU_cycles=max(x['max_produce_CPU_cycles'] for x in dead['songs'])),indent=2))
