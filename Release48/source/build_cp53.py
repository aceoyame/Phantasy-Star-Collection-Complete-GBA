#!/usr/bin/env python3
"""Build CP53 DMA3 video blanking atop CP50 source."""
from pathlib import Path
import argparse,os,json,subprocess,sys
S=Path(__file__).resolve().parent;R=S.parent
p=argparse.ArgumentParser()
p.add_argument('--out',type=Path,default=R/'binaries/RUN_THIS_R48_CP50_NATIVE_MONO.gba')
p.add_argument('--base',type=Path,default=R/'binaries/RUN_THIS_R48_CP07_SAFE_HANDOFF.gba')
a=p.parse_args();a.out.parent.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);u=R/'compiler/root/usr';bin=u/'lib/llvm-18/bin'
if (bin/'clang').exists():
 env['PATH']=str(bin)+':'+env.get('PATH','')
 env['LD_LIBRARY_PATH']=str(u/'lib/x86_64-linux-gnu')+(':'+env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
 for key,name in [('PSC8_CC','clang'),('PSC8_LD','ld.lld'),('PSC8_OBJCOPY','llvm-objcopy')]:env.setdefault(key,str(bin/name))
def run(*args):subprocess.run([sys.executable,*map(str,args)],env=env,check=True)
run(S/'tools/verify_cp22_wave_layout.py',R)
run(S/'tools/verify_cp27_text_indexes.py',R)
run(S/'psc8/core/gen_asm.py',S/'psc8/z80a.s')
run(S/'build_cp15.py','--base',a.base,'--out',a.out)
d=json.loads((R/'build_artifacts/CP15_BUILD_REPORT.json').read_text())
d['checkpoint']='CP53';d['parent']='CP52 1D03EC76'
d['changes']=[
 'Retain CP50 validated native mono/stereo audio paths, CP49 FM and CP47 palette renderer',
 'Retain CP52 DMA3 framebuffer clear when VDP display disabled',
 'Skip dirty VRAM scans when no VRAM blocks changed; skip sprite-table parse if SAT unchanged and nonwrapping',
 'Host reference fallback retains CPU clear for MD_HOST']
(R/'build_artifacts/CP53_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP53 build complete:',a.out)
