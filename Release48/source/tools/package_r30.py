#!/usr/bin/env python3
"""Clean source/donor/binary checkpoint, with no hashes or broad testing."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import argparse
R=Path(__file__).resolve().parents[1];tree=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.parent.mkdir(parents=True,exist_ok=True)
allowed_docs={'R30_NOTES.md','R30_BUILD_LIMITS.md','R30_SUBSYSTEM_AUDIT.md','R30_SUBSYSTEM_AUDIT.json','R19_SKILL_CONTROLLER_AUDIT.md'}
written=0
with ZipFile(a.out,'w',ZIP_DEFLATED,compresslevel=2) as z:
 for p in sorted(R.rglob('*')):
  if not p.is_file():continue
  rel=p.relative_to(R)
  if any(x=='__pycache__' or x.startswith('build-') or x.startswith('build_') and x!='build_r30.py' and p.is_dir() for x in rel.parts):continue
  if p.suffix in ('.o','.pyc','.gba','.elf','.map','.su','.zip') or '.pre_' in p.name:continue
  if 'docs' in rel.parts and p.name not in allowed_docs:continue
  if len(rel.parts)==1 and p.suffix.lower() in ('.md','.txt','.json','.diff','.log'):continue
  z.write(p,'R30/source/'+rel.as_posix());written+=1
 for p in [tree/'START_HERE.md',tree/'binaries/RUN_THIS_R30_PSC_FIELD_CONTROLLERS.gba',tree/'donors/PSC_R15_shell_8MiB.gba']:
  z.write(p,'R30/'+p.relative_to(tree).as_posix());written+=1
 for name in ['R30_BUILD.log','R30_PACK.log','r30_controllers_probe.c']:
  p=tree/'build_artifacts'/name
  if p.exists():z.write(p,'R30/build_artifacts/'+name);written+=1
print('R30 clean handoff:',written,'files;',a.out.stat().st_size,'bytes;',a.out)
