#!/usr/bin/env python3
"""Rebuild the complete CP26 collection with accepted native speed changes."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parent
ap=argparse.ArgumentParser();ap.add_argument('--out',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP26_NIBBLE_NATIVE.gba');ap.add_argument('--base',type=Path,default=ROOT/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba');a=ap.parse_args()
subprocess.run([sys.executable,str(R/'tools/verify_cp22_wave_layout.py'),str(ROOT)],check=True)
subprocess.run([sys.executable,str(R/'psc8/core/gen_asm.py'),str(R/'psc8/z80a.s')],check=True)
subprocess.run([sys.executable,str(R/'build_cp15.py'),'--base',str(a.base),'--out',str(a.out)],check=True)
d=json.loads((ROOT/'build_artifacts/CP15_BUILD_REPORT.json').read_text());d['checkpoint']='CP26'
d['changes']=['CP25 index and all earlier native CPU/graphics/sound/PSC8 improvements retained','Event-bounded native RAM nibble compositor preserves 96..128-cycle conditional iterations','Empty plane rows skip flip/palette work; palette and priority attributes use exact bit extraction','Native fusion requires the complete original loop body to match; short budgets and unsupported addresses retain generated instructions']
(ROOT/'build_artifacts/CP26_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP26 build complete',a.out)
