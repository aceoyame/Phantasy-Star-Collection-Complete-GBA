#!/usr/bin/env python3
"""Rebuild the complete CP27 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP27_NATIVE_KERNING.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'tools/verify_cp27_text_indexes.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP27'
d['changes']=['CP26 and all earlier native CPU, graphics, sound and PSC8 improvements retained','Verified immutable kerning index and direct character-class lookups replace translated searches','Event-bounded searches retain 26-cycle nonmatches and exact 14/24-cycle matching/terminator exits','Closed-form arithmetic shifts and ordinary rotates replace repeated single-bit AOT operations','Full original-loop fingerprints, immutable-table checks and instruction/event fallbacks retained']
(ROOT/'build_artifacts/CP27_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP27 build complete',a.out)
