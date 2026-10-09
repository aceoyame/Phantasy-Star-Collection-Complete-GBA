#!/usr/bin/env python3
"""Focused restoration comparisons, with no project hashes."""
from pathlib import Path
import argparse,json
p=argparse.ArgumentParser();p.add_argument('--original',type=Path,required=True);p.add_argument('--restored',type=Path,required=True);p.add_argument('--after-build',action='store_true');a=p.parse_args()
def source_files(root):return {p.relative_to(root/'source'):p for p in (root/'source').rglob('*') if p.is_file() and not {'__pycache__','build-psc-module'}.intersection(p.parts)}
f=source_files(a.original);g=source_files(a.restored);assert f.keys()==g.keys()
for name in f:assert f[name].read_bytes()==g[name].read_bytes(),name
out={'restored_source_files':len(f),'source_files_byte_exact':True,'scope':'Exact bytes and archive CRCs; no hashes.'}
if a.after_build:
 for rel in ['source/build-psc-module/ps4_psc_module.raw.gba','binaries/RUN_THIS_R44_PSC_MONSEN_ALIS_SWORD.gba','donors/PSC_R15_shell_8MiB.gba']:
  assert (a.original/rel).read_bytes()==(a.restored/rel).read_bytes(),rel
 out.update(native_module_byte_exact=True,rom_byte_exact=True,rom_bytes=(a.original/'binaries/RUN_THIS_R44_PSC_MONSEN_ALIS_SWORD.gba').stat().st_size)
(a.original/'build_artifacts/R44_RECOVERY.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS recovery:',out)
