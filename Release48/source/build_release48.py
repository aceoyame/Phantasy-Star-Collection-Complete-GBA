#!/usr/bin/env python3
"""Build Release 48 from the preserved native source/donor and accepted audio."""
import argparse,os,struct,subprocess,sys
from pathlib import Path
R=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--shell',type=Path,default=R.parent/'donors/PSC_R15_shell_8MiB.gba');ap.add_argument('--out',type=Path,default=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');ap.add_argument('--pack-only',action='store_true');grp=ap.add_mutually_exclusive_group();grp.add_argument('--native-audio-q1',action='store_true',help='apply conservative PSII/PSIII signed round-to-nearest mixer patch');grp.add_argument('--native-audio-wave12',action='store_true',help='apply zero-runtime 12.5% native II/III wave preblend plus signed round-to-nearest');grp.add_argument('--native-audio-wave12r',action='store_true',help='apply 12.5% native II/III wave preblend with bounded per-wave RMS retention plus signed round-to-nearest');a=ap.parse_args()
shell=a.shell.resolve().read_bytes()[:0x800000];sys.path.insert(0,str(R/'tools'))
from build_psc_four_game import slot4_handoff_code,SLOT4_HANDOFF_OFF,SLOT4_HANDOFF_ADDR,LAUNCH_INDEX4
assert len(shell)==0x800000 and struct.unpack_from('<I',shell,LAUNCH_INDEX4)[0]==SLOT4_HANDOFF_ADDR
assert shell[SLOT4_HANDOFF_OFF:SLOT4_HANDOFF_OFF+64]==slot4_handoff_code()
env=dict(os.environ,PS4_SKIP_ASSET_REGEN='1',PS4_NO_HASH='1');logdir=R.parent/'build_artifacts';logdir.mkdir(exist_ok=True)
if not a.pack_only:
 # Existing immutable actor assets suffice; no asset regeneration.
 with (logdir/'R46_BUILD.log').open('w') as log:
  subprocess.run([sys.executable,str(R/'build_v098.py'),'--rom',str(R/'references/roms/ps4built.bin'),'--logo-from',str(a.shell.resolve()),'--psc-module'],cwd=R,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
from release48_startup import apply as clean_startup
shell=clean_startup(shell)
if a.native_audio_q1:
 from release48_native_audio_quality import apply as improve_native_audio
 shell=improve_native_audio(shell)
elif a.native_audio_wave12 or a.native_audio_wave12r:
 from release48_native_wave_smooth import apply as smooth_native_audio
 from release48_native_audio_quality import apply as improve_native_audio
 shell,_=smooth_native_audio(shell,1,8,preserve_rms=a.native_audio_wave12r,gain_cap=1.08)
 shell=improve_native_audio(shell)
module_path=R/'build-psc-module/ps4_psc_module.raw.gba'
if module_path.is_file():
 module=module_path.read_bytes();assert len(module)<=0x1800000
 tail=module+b'\xff'*(0x1800000-len(module))
 module_len=len(module)
else:
 if not a.pack_only:
  raise FileNotFoundError(module_path)
 # Clean release archives intentionally omit the large intermediate module.
 # For shell-only repacks, reuse the already-accepted 24 MiB PSIV tail verbatim.
 accepted=(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba').read_bytes()
 assert len(accepted)==0x2000000
 tail=accepted[0x800000:]
 module_len=len(tail.rstrip(b'\xff'))
a.out=a.out.resolve();a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_bytes(shell+tail)
print('PASS Release 48 module',module_len,'headroom',0x1800000-module_len);print(a.out)
