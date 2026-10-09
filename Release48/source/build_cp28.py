#!/usr/bin/env python3
"""Rebuild the complete CP28 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP28_NATIVE_GLYPH.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'tools/verify_cp27_text_indexes.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP28'
d['changes']=['CP27 and all earlier CPU, graphics, sound and PSC8 improvements retained', 'Event-bounded native font expansion with palette/cycle lookup retains original 80/84/88-cycle iterations, flags and writes', 'Plane metadata is decoded once per tile band; uniform and blank rows reuse a single decoded tile', 'Hot-memory checked line copies and DMA3 transfers retain exact framebuffer and blank-image state', 'Font and RAM byte-copy helpers use compact Thumb code in ROM; guest timing and all fallbacks retained']
(ROOT/'build_artifacts/CP28_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP28 build complete',a.out)
