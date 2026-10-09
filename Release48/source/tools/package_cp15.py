#!/usr/bin/env python3
"""One complete continuation package; no hashes during checkpoint assembly."""
from pathlib import Path
import argparse,json,zipfile
R=Path(__file__).resolve().parents[2];W=R.parent
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);p.add_argument('--compiler',type=Path);a=p.parse_args()
out=a.out.resolve();out.parent.mkdir(parents=True,exist_ok=True);tmp=out.with_suffix('.partial')
allowed_roms={'RUN_THIS_R48_CP07_SAFE_HANDOFF.gba','RUN_THIS_R48_CP11_TEST.gba','RUN_THIS_R48_CP12_TEST.gba','RUN_THIS_R48_CP13_TEST.gba','RUN_THIS_R48_CP14_TEST.gba','RUN_THIS_R48_CP15_TEST.gba'}
with zipfile.ZipFile(tmp,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 z.write(R/'binaries/RUN_THIS_R48_CP15_TEST.gba','RUN_THIS_R48_CP15_TEST.gba')
 z.write(W/'CP15_NOTES.md','START_HERE.md')
 for file in sorted(R.rglob('*')):
  if not file.is_file():continue
  rel=file.relative_to(R)
  if any(s in ('__pycache__','.git','toolchain_archives') for s in rel.parts):continue
  if file.suffix in ('.o','.so','.pyc','.partial'):continue
  if file.suffix=='.gba' and not (rel.parts[0]=='binaries' and file.name in allowed_roms):continue
  z.write(file,'Release48/'+rel.as_posix())
 for name in ['validation_cp15','validation_cp14','validation_cp13','validation_cp12','00_START_HERE']:
  for file in sorted((W/name).rglob('*')):
   if file.is_file() and file.suffix and '__pycache__' not in file.parts and file.suffix not in ('.raw','.wav','.gba','.o','.so','.pyc','.state'):
    z.write(file,name+'/'+file.relative_to(W/name).as_posix())
 for name in ['CP15_NOTES.md','CP14_NOTES.md','CP13_NOTES.md','CP11_NOTES.md','CP10_SPEED_ANALYSIS.md','PS23_AUDIO_PROBLEM_STATEMENT.md']:
  if (W/name).is_file():z.write(W/name,name)
 if a.compiler:
  packages=json.loads((R/'source/tools/clang18_packages.json').read_text())
  for info in packages:
   file=a.compiler/Path(info['filename']).name
   assert file.stat().st_size==info['size']
   z.write(file,'Release48/toolchain_archives/'+file.name,compress_type=zipfile.ZIP_STORED)
tmp.replace(out)
print('Packaged',out,'bytes',out.stat().st_size)
