#!/usr/bin/env python3
"""Host diagnostic: log exact production note events without editing ROM code."""
import argparse, json, shutil, subprocess, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--song',default='0xac');ap.add_argument('--frames',type=int,default=3600);ap.add_argument('--out-dir',type=Path,required=True);ap.add_argument('--profile',choices=['current','reference04','reference05','release03'],default='current');a=ap.parse_args()
root=Path(tempfile.mkdtemp(prefix='r46_note_trace_'));a.out_dir.mkdir(parents=True,exist_ok=True)
audio=R/'gba/audio_p134r'
if a.profile!='current':
 for target,record in json.loads((R.parent/'AUDIO_BANK_PROFILES.json').read_text())['profiles'][a.profile]['files'].items():
  dst=root/target;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R.parent/record['file'],dst)
 audio=root/'source/gba/audio_p134r'
source=(audio/'src/chip_audio.cpp').read_text()
declaration='extern unsigned trace_frame, trace_song;\nvoid trace_note(unsigned,unsigned,unsigned,unsigned,unsigned,int,unsigned,unsigned,unsigned,unsigned);\n'
source=declaration+source
marker='        lane.previous_duration = duration;'
assert source.count(marker)==1
source=source.replace(marker,'        trace_note(unsigned(&lane-lanes),lane.pc,unsigned(lane.kind),event,duration,lane.current_note,unsigned(lane.keyed),unsigned(held),lane.instrument,unsigned(static_cast<unsigned char>(multiplied)));\n'+marker)
chip=root/'chip.cpp';chip.write_text(source)
cpp=root/'trace.cpp';cpp.write_text((R/'tools_host/audio_sim/trace_notes.cpp').read_text().replace('TRACE_CHIP_PATH',str(chip)))
flags=['-D'+s+'=1' for s in ['MD2GBA_PS4_MAXMOD_MODE_A','MD2GBA_PS4_MAXMOD_CHANNEL_MAJOR','MD2GBA_PS4_V19_FULL_MIX','MD2GBA_PS4_M4A_NATIVE_ONLY']]
subprocess.run(['g++','-O2','-std=c++17',*flags,'-I'+str(audio/'include'),'-I'+str(audio/'src'),'-I'+str(R/'gba/audio_p134r/include'),str(cpp),str(R/'gba/audio_p134r/src/ps4_chip_audio_data.cpp'),str(audio/'src/ps4_fm_multisample_data.cpp'),'-o',str(root/'trace')],check=True)
with (a.out_dir/'notes.tsv').open('w') as log:subprocess.run([str(root/'trace'),a.song,str(a.frames),str(a.out_dir/'opening.s8')],stdout=log,check=True)
print('Production note trace:',a.out_dir,flush=True)
