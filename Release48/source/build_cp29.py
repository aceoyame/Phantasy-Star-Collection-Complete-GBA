#!/usr/bin/env python3
"""Rebuild the complete CP29 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP29_NATIVE_ROWS.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'tools/verify_cp27_text_indexes.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP29'
d['changes']=['CP28 and earlier CPU, graphics, sound and PSC8 improvements retained', 'Native event-bounded font-row gather at 0x2BC8A retains full 28-cycle MOVE.L/ADDA.W/DBRA iterations', 'Native shifted-nibble compositor at 0x2BC4C retains 76/86/114/124-cycle iterations, first-write/second-read overlap, flags and one/two write counts', 'Both new Thumb helpers reside in ROM; unsupported buses, wrapping transfers and short event budgets retain original generated instructions', 'Native fusion fingerprints reproduce the exact accepted generated source and refuse changed loop bodies; guest timing, PCM, pixels and saves retained']
(ROOT/'build_artifacts/CP29_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP29 build complete',a.out)
