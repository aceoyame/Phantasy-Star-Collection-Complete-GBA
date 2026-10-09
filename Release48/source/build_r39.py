#!/usr/bin/env python3
"""Build R39 from the preserved R38 source and PSC donor and accepted audio."""
import argparse,os,struct,subprocess,sys
from pathlib import Path
R=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--shell',type=Path,default=R.parent/'donors/PSC_R15_shell_8MiB.gba');ap.add_argument('--out',type=Path,default=R.parent/'binaries/RUN_THIS_R39_PSC_LUTZ_AIR_SETH.gba');ap.add_argument('--pack-only',action='store_true');a=ap.parse_args()
shell=a.shell.resolve().read_bytes()[:0x800000];sys.path.insert(0,str(R/'tools'))
from build_psc_four_game import slot4_handoff_code,SLOT4_HANDOFF_OFF,SLOT4_HANDOFF_ADDR,LAUNCH_INDEX4
assert len(shell)==0x800000 and struct.unpack_from('<I',shell,LAUNCH_INDEX4)[0]==SLOT4_HANDOFF_ADDR
assert shell[SLOT4_HANDOFF_OFF:SLOT4_HANDOFF_OFF+64]==slot4_handoff_code()
env=dict(os.environ,PS4_SKIP_ASSET_REGEN='1',PS4_NO_HASH='1');logdir=R.parent/'build_artifacts';logdir.mkdir(exist_ok=True)
if not a.pack_only:
 # Existing immutable actor assets suffice; no asset regeneration.
 with (logdir/'R39_BUILD.log').open('w') as log:
  subprocess.run([sys.executable,str(R/'build_v098.py'),'--rom',str(R/'references/roms/ps4built.bin'),'--logo-from',str(a.shell.resolve()),'--psc-module'],cwd=R,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
module=(R/'build-psc-module/ps4_psc_module.raw.gba').read_bytes();assert len(module)<=0x1800000
a.out=a.out.resolve();a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(shell+module+b'\xff'*(0x1800000-len(module)))
print('PASS R39 module',len(module),'headroom',0x1800000-len(module));print(a.out)
