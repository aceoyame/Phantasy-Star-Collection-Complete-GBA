#!/usr/bin/env python3
"""Make a small cumulative R40 recovery checkpoint against the preserved R39."""
from pathlib import Path
import argparse,json,zipfile
ap=argparse.ArgumentParser()
ap.add_argument('--baseline',type=Path,required=True)
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--label',required=True)
args=ap.parse_args()
root=Path(__file__).resolve().parents[2]
changes=[]
skip={'build-psc-module','__pycache__'}
for p in sorted((root/'source').rglob('*')):
 if not p.is_file() or skip.intersection(p.parts):continue
 rel=p.relative_to(root/'source');old=args.baseline/rel
 if old.exists():
  a,b=p.stat(),old.stat()
  if a.st_size==b.st_size and a.st_mtime_ns==b.st_mtime_ns:continue
  if p.read_bytes()==old.read_bytes():continue
 changes.append(p)
changes.extend(p for p in sorted((root/'build_artifacts').glob('*')) if p.is_file() and p.suffix in {'.md','.json','.log','.map','.c','.png','.csv'})
args.out.parent.mkdir(exist_ok=True,parents=True)
staged=args.out.with_name(args.out.name+'.tmp')
with zipfile.ZipFile(staged,'w',zipfile.ZIP_DEFLATED) as z:
 for p in changes:z.write(p,'R40/'+p.relative_to(root).as_posix())
 manifest={'checkpoint':args.label,'cumulative_against':'R39 final source','files':['R40/'+p.relative_to(root).as_posix() for p in changes],
  'restore':'Restore R32 full checkpoint 01, R32 increment 03, R33 increment 04, R34 increment 04, R35 increment 04, R36 increment 04, R37 increment 04, R38 increment 04, R39 increment 04, then overlay this R40 increment 04. Use tools/restore_r40_source.py to strip the revision roots and overlay into one directory. Check docs/R40_PROGRESS.md before building. Older R40 increments are not needed.'}
 z.writestr('R40/CHECKPOINT_MANIFEST.json',json.dumps(manifest,indent=2)+'\n')
with zipfile.ZipFile(staged) as z:assert z.testzip() is None
staged.replace(args.out)
print(args.label,len(changes),'files',args.out.stat().st_size,'bytes',args.out)
