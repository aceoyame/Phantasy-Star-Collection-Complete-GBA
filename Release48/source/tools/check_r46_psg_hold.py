#!/usr/bin/env python3
"""Original-ROM envelope oracle and full production-mixer song sweep."""
import argparse,json,os,shutil,subprocess,tempfile,wave
from pathlib import Path
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);ap.add_argument('--baseline-zip',type=Path);ap.add_argument('--profile',choices=['current','release03','reference04','reference05'],default='current');ap.add_argument('--write-previews',action='store_true');a=ap.parse_args()
root=Path(tempfile.mkdtemp(prefix='r46_psg_hold_'));cpp=root/'check.cpp'
source=R/'gba/audio_p134r/src/chip_audio.cpp'
bank_source=R/'gba/audio_p134r/src/ps4_fm_multisample_data.cpp';includes=[]
if a.profile!='current':
 for target,record in json.loads((R.parent/'AUDIO_BANK_PROFILES.json').read_text())['profiles'][a.profile]['files'].items():
  dst=root/target;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R.parent/record['file'],dst)
 backup=root/'source/gba/audio_p134r'
 source=backup/'src/chip_audio.cpp';bank_source=backup/'src/ps4_fm_multisample_data.cpp';includes=['-I'+str(backup/'include'),'-I'+str(backup/'src')]
if a.baseline_zip:
 import zipfile
 with zipfile.ZipFile(a.baseline_zip) as z:
  for name in ['chip_audio.cpp','ps4_fm_multisample_data.cpp','ps4_fm_multisample_data_generated.inc','p134r_song_trim.h']:(root/name).write_bytes(z.read('R46/source/gba/audio_p134r/src/'+name))
  (root/'include/ps4gba').mkdir(parents=True);(root/'include/ps4gba/ps4_fm_multisample_data.h').write_bytes(z.read('R46/source/gba/audio_p134r/include/ps4gba/ps4_fm_multisample_data.h'))
 source=root/'chip_audio.cpp';bank_source=root/'ps4_fm_multisample_data.cpp';includes=['-I'+str(root/'include'),'-I'+str(root)]
cpp.write_text((R/'tools_host/audio_sim/check_psg_hold.cpp').read_text().replace('../../gba/audio_p134r/src/chip_audio.cpp',str(source)))
flags=['-D'+s+'=1' for s in ['MD2GBA_PS4_MAXMOD_MODE_A','MD2GBA_PS4_MAXMOD_CHANNEL_MAJOR','MD2GBA_PS4_V19_FULL_MIX','MD2GBA_PS4_M4A_NATIVE_ONLY']]
subprocess.run(['g++','-O1','-g','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',*flags,*includes,'-I'+str(R/'gba/audio_p134r/include'),'-I'+str(R/'gba/audio_p134r/src'),str(cpp),str(R/'gba/audio_p134r/src/ps4_chip_audio_data.cpp'),str(bank_source),'-o',str(root/'check')],check=True)
preview_dir=R.parent/'audio_previews'/a.label
if a.write_previews:preview_dir.mkdir(parents=True,exist_ok=True)
result=subprocess.run([str(root/'check'),str(R/'references/roms/ps4built.bin'),*([str(preview_dir)] if a.write_previews else [])],text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
if a.write_previews:
 import numpy as np
 for path in preview_dir.glob('*.s8'):
  pcm=np.frombuffer(path.read_bytes(),dtype=np.int8).astype('<i2')*256
  with wave.open(str(path.with_suffix('.wav')),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(13379);w.writeframes(pcm.tobytes())
  path.unlink()
rows=[json.loads(x) for x in result.stdout.splitlines() if x.startswith('{')]
report={'baseline':bool(a.baseline_zip),'bank_profile':a.profile,'sanitizers':'ASan/UBSan; LeakSanitizer disabled for host ptrace restrictions','return_code':result.returncode,'songs':rows[:-1],'summary':rows[-1] if rows else None,'stderr':result.stderr,'scope':'10 original-ROM envelopes, 128 controlled updates each; 52 songs x 1800 frames with actual native production flags and the selected bank; previews are isolated source-synth output, not captured final GBA transport; no perceptual or physical hardware certification'}
(O/('R46_'+a.label+'_PSG_HOLD.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report['summary']),flush=True);print(result.stderr,flush=True)
assert result.returncode==(1 if a.baseline_zip else 0),report
assert len(rows)==53,report
