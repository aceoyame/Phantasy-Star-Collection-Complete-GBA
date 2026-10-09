#!/usr/bin/env python3
"""Rebuild the complete CP30 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP30_NATIVE_CALLS.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'tools/verify_cp27_text_indexes.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP30'
d['changes']=['All CP29 and earlier CPU/graphics/sound/PSC8 improvements retained', 'Native event-bounded nametable MOVE.W/ADD.W/VDP write/DBRA iterations at 0x248A retain 28 guest cycles, individual device writes, flags and exact fallback', 'Native four-byte tile-packing leaf at 0x3B9C retains all registers/CCR, 88 guest cycles, a real stack return, overlap order and the VDP write at guest cycle 76', '57 measured interpreter-miss entry PCs translated; full-flag private fall-through tails preserve scanline/interrupt stack state; identical translations shared between engines', 'No added RAM, skipped guest execution or changed source cartridges; hardware accesses and interrupts checked with an independent reference-core transcript', 'Slower call-merging and ROM RAM-trampoline trials are excluded; production dispatcher remains byte-identical to CP29']
(ROOT/'build_artifacts/CP30_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP30 build complete',a.out)
