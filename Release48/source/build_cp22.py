#!/usr/bin/env python3
"""Rebuild the complete CP22 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP22_NATIVE_SPEED.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP22'
d['changes']=['Gaiden byte-verified native drawing and coordinate spans with exact event boundaries and mapper fallback','Incremental native cartridge mapping for PS1 and both Game Gear titles','Skip unused Game Gear FM accumulator clearing','Shared Genesis eight-pixel graphics decoder AOT','18 duplicate Genesis translations merged without changing function-table indices']
(ROOT/'build_artifacts/CP22_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP22 build complete',a.out)
