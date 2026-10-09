#!/usr/bin/env python3
"""Make a small cumulative R42 recovery checkpoint against the preserved R41."""
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
 for p in changes:z.write(p,'R42/'+p.relative_to(root).as_posix())
 manifest={'checkpoint':args.label,'cumulative_against':'R41 final source','files':['R42/'+p.relative_to(root).as_posix() for p in changes],
  'restore':'Restore R32 full checkpoint01, R32 increment03, R33/R34/R35/R36/R37/R38/R39/R40/R41 final increment04 in order, then overlay this R42 increment04. Use tools/restore_r42_source.py to strip revision roots. Read docs/R42_NOTES.md before building. Older R42 increments are not needed.'}
 z.writestr('R42/CHECKPOINT_MANIFEST.json',json.dumps(manifest,indent=2)+'\n')
with zipfile.ZipFile(staged) as z:assert z.testzip() is None
staged.replace(args.out)
print(args.label,len(changes),'files',args.out.stat().st_size,'bytes',args.out)
