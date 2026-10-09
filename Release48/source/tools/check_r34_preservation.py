#!/usr/bin/env python3
"""Focused byte comparisons for the accepted audio, shell and actor prefix."""
from pathlib import Path
import argparse,json,re
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--baseline',type=Path,required=True);a=ap.parse_args()
files=json.loads((a.baseline.parent/'build_artifacts/R33_PRESERVATION.json').read_text())['unchanged_audio_files']
for f in files:assert (R/f).read_bytes()==(a.baseline/f).read_bytes(),f
old=(a.baseline/'gba/r31_field_poses.bin').read_bytes();new=(R/'gba/r31_field_poses.bin').read_bytes();assert new[:len(old)]==old
gba=(R.parent/'binaries/RUN_THIS_R34_PSC_STORY_CONTINUATIONS.gba').read_bytes()
donor=(R.parent/'donors/PSC_R15_shell_8MiB.gba').read_bytes()[:0x800000];assert gba[:0x800000]==donor
assert donor==(a.baseline.parent/'donors/PSC_R15_shell_8MiB.gba').read_bytes()[:0x800000]
module=(R/'build-psc-module/ps4_psc_module.raw.gba').read_bytes();assert len(gba)==0x2000000 and len(module)<=0x1800000
assert gba[0x800000:0x800000+len(module)]==module and all(v==255 for v in gba[0x800000+len(module):])
maptext=(R/'build-psc-module/ps4_psc_module.map').read_text()
def symbol(name):
 m=re.search(r'^\s*([0-9a-fA-F]+)\s+[^\n]*\b'+name+r'\s*=',maptext,re.M);assert m,name;return int(m[1],16)
iw,ew=symbol('__iwram_alloc_end'),symbol('__bss_end');assert iw<=0x3006D00 and ew<=0x202A000
report={'unchanged_audio_files':files,'launcher_byte_identical':True,'retained_r33_pose_bank_bytes':len(old),'appended_r34_pose_bytes':len(new)-len(old),
 'limits':{'iwram_end':hex(iw),'iwram_before_irq_reserve_bytes':0x3006D00-iw,'ewram_bss_end':hex(ew),'ewram_before_presentation_arena_bytes':0x202A000-ew,'module_bytes':len(module),'slot_headroom_bytes':0x1800000-len(module),'rom_bytes':len(gba)},
 'scope':'Focused byte comparisons and linked boundaries only; no hashes, no emulator/hardware playthrough.'}
(O/'R34_PRESERVATION.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(files),'accepted audio files and 8 MiB PSC shell byte-identical; R33 pose prefix retained')
print('PASS 32 MiB ROM packaging, ROM/RAM bounds:',report['limits'])
