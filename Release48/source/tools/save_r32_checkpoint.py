#!/usr/bin/env python3
"""Package a durable, self-contained checkpoint; excludes reproducible caches."""
from pathlib import Path
import argparse,json,zipfile
R=Path(__file__).resolve().parents[1];tree=R.parent
ap=argparse.ArgumentParser();ap.add_argument('number');ap.add_argument('--out-dir',type=Path,required=True);a=ap.parse_args();a.out_dir.mkdir(parents=True,exist_ok=True)
manifest={'release':'R32','checkpoint':a.number,'status':'See R32_CHECKPOINT_STATUS.md; checkpoint is not a full-game certification','binary':'binaries/RUN_THIS_R32_PSC_TRAVEL_AND_MAPS.gba','rebuild':'source/build_r32.py','validation':'source/tools/check_r32_travel_maps.py','baseline':'R31 saved handoff'}
(tree/'HANDOFF_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
out=a.out_dir/f'PSIV_R32_CHECKPOINT_{a.number}_2026-09-30.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=1) as z:
 for p in sorted(tree.rglob('*')):
  if not p.is_file():continue
  rel=p.relative_to(tree)
  if any(x in rel.parts for x in ['__pycache__','build-psc-module','.git']):continue
  if p.name.endswith('.baseline.c') or p.name in ['r31_story_probe','r32_travel_maps_probe','r32_vehicle_probe']:continue
  if rel.parts[0]=='binaries' and p.name!='RUN_THIS_R32_PSC_TRAVEL_AND_MAPS.gba':continue
  z.write(p,'R32/'+str(rel))
print(out,out.stat().st_size)
payload={'uploads':[{'local_path':str(out),'purpose':'create_library_file','library_artifact_type':'other'}]}
(a.out_dir/f'checkpoint{a.number}_upload.json').write_text(json.dumps(payload))
