#!/usr/bin/env python3
"""Structural guard for zero-runtime PSII/PSIII wave-preconditioning builds."""
from pathlib import Path
import argparse, json, sys, zlib
R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/'source/tools'))
from release48_native_wave_smooth import _wave_intervals
from release48_native_audio_quality import MIXER_OFFSETS, OLD_MIXER, NEW_MIXER
ap=argparse.ArgumentParser(); ap.add_argument('base',type=Path); ap.add_argument('candidate',type=Path); ap.add_argument('--out',type=Path); a=ap.parse_args()
base=a.base.read_bytes(); cand=a.candidate.read_bytes()
assert len(base)==len(cand)==0x2000000
shell=base[:0x800000]
allowed=bytearray(0x800000)
intervals={}
for game in ('II','III'):
    uniq={}
    for r in _wave_intervals(shell,game): uniq[(r.off,r.length)]=r
    intervals[game]=[]
    for off,length in uniq:
        allowed[off:off+length]=b'\x01'*length
        intervals[game].append((off,length))
for game,off in MIXER_OFFSETS.items():
    allowed[off:off+len(OLD_MIXER)]=b'\x01'*len(OLD_MIXER)
    assert cand[off:off+len(NEW_MIXER)]==NEW_MIXER, f'{game}: expected quantizer mixer missing'
# Appended PSIV module must remain byte-identical.
assert base[0x800000:]==cand[0x800000:], 'PSIV/appended module changed'
changed=[i for i,(x,y) in enumerate(zip(base[:0x800000],cand[:0x800000])) if x!=y]
outside=[i for i in changed if not allowed[i]]
assert not outside, f'changes outside native II/III wave/mixer scope: first {outside[:8]}'
# Table bytes and phase kernels are deliberately fixed.
assert base[0x481510:0x481510+256*8]==cand[0x481510:0x481510+256*8]
assert base[0x152080:0x152080+256*60]==cand[0x152080:0x152080+256*60]
assert base[0x3955B8:0x395700]==cand[0x3955B8:0x395700]
assert base[0x0831C8:0x083400]==cand[0x0831C8:0x083400]
counts={}
for game,items in intervals.items():
    s=0
    for off,length in items:
        s+=sum(base[j]!=cand[j] for j in range(off,off+length))
    counts[game]=s
report={
 'base_crc32':f'{zlib.crc32(base)&0xffffffff:08x}',
 'candidate_crc32':f'{zlib.crc32(cand)&0xffffffff:08x}',
 'changed_shell_bytes':len(changed),
 'changed_wave_bytes':counts,
 'mixer_changed_bytes':{g:sum(base[off+i]!=cand[off+i] for i in range(len(OLD_MIXER))) for g,off in MIXER_OFFSETS.items()},
 'outside_allowed_scope_changes':len(outside),
 'psiv_appended_region_byte_identical':True,
 'native_tables_byte_identical':True,
 'phase_kernels_byte_identical':True,
}
if a.out:
    a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_text(json.dumps(report,indent=2)+'\n')
print('PASS zero-runtime native-wave structural scope')
print(json.dumps(report,indent=2))
