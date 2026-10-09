#!/usr/bin/env python3
"""Build the accepted CP46 live native sequencer checkpoint from supplied inputs."""
from pathlib import Path
import argparse,os,json,subprocess,sys
S=Path(__file__).resolve().parent;R=S.parent
p=argparse.ArgumentParser()
p.add_argument('--out',type=Path,default=R/'binaries/RUN_THIS_R48_CP46_NATIVE_SEQUENCER.gba')
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
d['checkpoint']='CP46';d['parent']='CP45 F2D4D372'
d['changes']=[
 'Native common playback housekeeping at 00E5, with live variable tempo and all stack residues',
 'Native complete inactive-channel scheduler iterations at 0128',
 'Native continuing FM note path at 0139, with exact duration and modulation state',
 'Native complete signed pitch/octave routine at 0434 shared by three callers',
 'Original guest cycles, refresh register, flags, return-address RAM writes and event fallback retained',
 'CP45 command routines, all prior CPU/render/PS1/Gaiden improvements and every cartridge retained']
(R/'build_artifacts/CP46_BUILD_REPORT.json').write_text(json.dumps(d,indent=2)+'\n')
print('CP46 build complete:',a.out)
