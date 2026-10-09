#!/usr/bin/env python3
"""Rebuild the complete CP25 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP25_ADVENTURE_INDEX.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP25'
d['changes']=['CP24 native CPU/graphics and earlier PSC8 improvements retained','Conservative tile-to-visible-band index replaces repeated plane/window reference scans for aligned vscroll','Native clipped dirty-line bit masks and merged band expansion','Wrapped name-table dirty detection and full-height line-scroll invalidation','Aligned halfword/word RAM copy/fill paths preserve forward overlap, guest cycles and write generations']
(ROOT/'build_artifacts/CP25_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP25 build complete',a.out)
