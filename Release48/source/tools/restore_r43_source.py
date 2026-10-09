#!/usr/bin/env python3
"""Restore cumulative R32→R43 checkpoints into one complete R43 directory."""
from pathlib import Path
import argparse,zipfile
p=argparse.ArgumentParser();p.add_argument('--archives',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
chain=['PSIV_R32_CHECKPOINT_01_2026-09-30.zip','PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip','PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip','PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip','PSIV_R35_CHECKPOINT_04_INCREMENT_2026-09-30.zip','PSIV_R36_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R37_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R38_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R39_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R40_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R41_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R42_CHECKPOINT_04_INCREMENT_2026-10-01.zip','PSIV_R43_CHECKPOINT_04_INCREMENT_2026-10-01.zip']
a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
for name in chain:
 with zipfile.ZipFile(a.archives/name) as z:
  assert z.testzip() is None,name
  for info in z.infolist():
   path=Path(info.filename);assert not path.is_absolute() and '..' not in path.parts
   if info.is_dir() or len(path.parts)<2:continue
   target=a.out/Path(*path.parts[1:]);target.parent.mkdir(parents=True,exist_ok=True)
   with z.open(info) as source,target.open('wb') as out:
    while data:=source.read(1048576):out.write(data)
 print('restored',name,flush=True)
assert (a.out/'source/gba/r43_elsydeon_reunion.h').exists() and (a.out/'source/gba/ps4_reversed.bin').stat().st_size==0x300000
print('PASS complete R43 source restored; read source/docs/R43_NOTES.md')
