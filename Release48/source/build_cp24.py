#!/usr/bin/env python3
"""Rebuild the complete CP24 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP24_ADVENTURE_SPEED.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP24'
d['changes']=['CP22 native PSC8 kernels, incremental mapper and Genesis decoder retained','CP23 exact divide-by-18 and branchless PCM saturation merged','Aligned single-plane tile rows decoded as paired native pixels','Dirty pattern scans restricted to visible columns for full-screen scrolling','Four event-bounded 68000 byte copy/fill loops use native RAM spans','Exact ARM reciprocal divide-by-12 replaces software division in fill spans']
(ROOT/'build_artifacts/CP24_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP24 build complete',a.out)
