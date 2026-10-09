#!/usr/bin/env python3
"""Restore the exact R35 ROM using only Python's standard library.
Usage: python3 restore_r35.py /path/to/PSIV_R34_ROM_2026-09-30.zip
The baseline may also be the unzipped RUN_THIS_R34_PSC_STORY_CONTINUATIONS.gba.
Keep r35_delta.bin and r34_samples.bin alongside this script.
"""
from pathlib import Path
import argparse,struct,zipfile
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('baseline',type=Path)
ap.add_argument('--out',type=Path,default=P/'RUN_THIS_R35_PSC_WREN_CRASH.gba')
a=ap.parse_args()
if a.baseline.suffix.lower()=='.zip':
 with zipfile.ZipFile(a.baseline) as z:
  names=[n for n in z.namelist() if Path(n).name=='RUN_THIS_R34_PSC_STORY_CONTINUATIONS.gba']
  if len(names)!=1:ap.error('Use the original R34 ROM archive.')
  base=z.read(names[0])
else:base=a.baseline.read_bytes()
if len(base)!=0x2000000:ap.error('The R34 baseline must be exactly 32 MiB.')
samples=(P/'r34_samples.bin').read_bytes()
for offset in range(0,len(base),65536):
 expected=samples[(offset//65536)*32:(offset//65536+1)*32]
 if base[offset:offset+32]!=expected:ap.error(f'R34 baseline bytes differ at 0x{offset:X}; use the saved R34 ROM.')
patch=(P/'r35_delta.bin').read_bytes()
if patch[:12]!=b'PSC-R35-DLT1':ap.error('Invalid R35 recovery data.')
length=struct.unpack_from('<I',patch,12)[0];pos=16;out=bytearray()
while pos<len(patch):
 op=patch[pos:pos+1];pos+=1
 if op==b'E':break
 if op==b'C':
  offset,n=struct.unpack_from('<II',patch,pos);pos+=8
  if offset+n>len(base):ap.error('Recovery copy exceeds the baseline.')
  out.extend(base[offset:offset+n])
 elif op==b'L':
  n=struct.unpack_from('<I',patch,pos)[0];pos+=4
  if pos+n>len(patch):ap.error('Recovery data is truncated.')
  out.extend(patch[pos:pos+n]);pos+=n
 else:ap.error('Invalid recovery instruction.')
 if len(out)>length:ap.error('Recovery output exceeds the declared ROM size.')
if pos!=len(patch) or len(out)!=length or length!=0x2000000:ap.error('Incomplete R35 ROM recovery.')
a.out.resolve().parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(out)
print('Restored 32 MiB R35 ROM:',a.out.resolve())
