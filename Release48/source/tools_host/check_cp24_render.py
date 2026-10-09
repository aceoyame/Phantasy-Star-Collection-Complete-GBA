#!/usr/bin/env python3
"""Compare renderer output to the original per-pixel renderer."""
from pathlib import Path
import subprocess
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp24'
files=[R/'source/tools_host/check_cp24_render.c',*[A/p for p in ['md.c','mdrender.c','m68k.c','m68k_ref.c','mddebug.c']]]
p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-ffunction-sections','-fdata-sections','-I'+str(A),'-I'+str(A/'aot'),*map(str,files),'-Wl,--gc-sections','-o',str(V/'md_render_check')],capture_output=True,text=True)
assert p.returncode==0,p.stderr
lines=[]
for args in [['fuzz','200'],['memo'],['single'],['dirty']]+[['incr',str(R/f'additional_roms/{name}_Adventure_-_English.bin'),'700'] for name in ['Anne','Rudger','Kinds','Shilka','Amia','Huey','Eusis','Nei']]:
 p=subprocess.run([str(V/'md_render_check'),*args],capture_output=True,text=True,check=True);lines.append(p.stdout);print(p.stdout.strip(),flush=True)
(V/'RENDER_EXACT.txt').write_text(''.join(lines))
