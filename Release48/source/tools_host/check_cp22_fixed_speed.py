#!/usr/bin/env python3
"""Measure identical guest-frame/input windows in the actual GBA binaries.
Debug breaks only synchronize scripted pad inputs; the virtual clock runs
normally and no guest cycles, samples or frames are skipped by the harness.
"""
from pathlib import Path
import os,sys,json,subprocess
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp22'
if len(sys.argv)>1:
 tag,index=sys.argv[1:];index=int(index)
 sys.path.insert(0,str(R/'source/tools_host/cp09_prof'));from gtest import boot,sym,L
 rom=R/'binaries/RUN_THIS_R48_CP21_ADVENTURE_NATIVE_DIV20.gba' if tag=='before' else R/'binaries/RUN_THIS_R48_CP22_NATIVE_SPEED.gba'
 g=boot(rom,index);entry=sym('md_begin_frame_input');machine=sym('M');zram=sym('md_zram');times={}
 for frame in range(1051):
  L.mg_break(entry);g.run(100);assert L.mg_hit(),('input',frame)
  ret=g.reg(14)&~1;L.mg_break(ret);g.run(100);assert L.mg_hit(),('input return',frame)
  # Same GBA-to-Genesis mapping as md_begin_frame_input, applied at its return.
  keys=8 if 180<=frame<184 else 2 if 400<=frame<404 else 8 if 500<=frame<504 else 1 if frame>=600 and frame%80<4 else 0
  pad=((keys>>6)&3)|((keys&32)>>3)|((keys&16)>>1)|((keys&2)<<3)|((keys&1)<<5)|((keys&256)>>2)|((keys&8)<<4)
  g.wr16(machine+44,pad)
  if frame==150:g.wr8(zram+0x1c0a,0x81)
  if frame in [150,450,750,1050]:times[frame]=g.cycles()
  if frame in [450,1050]:g.shot(V/f'FIXED_{index}_{tag}_{frame}.png')
 rows=[]
 for start in [150,450,750]:
  cycles=times[start+300]-times[start];rows.append({'guest_start':start,'guest_end':start+300,'native_cycles':cycles,'guest_per_GBA_frame':300*280896/cycles})
 print(json.dumps(rows));sys.exit(0)
rows=[]
for index in [2,9]:
 one={'index':index}
 for tag in ['before','after']:
  elf=R/'source/references/cp21_final/md.elf' if tag=='before' else R/'source/additional/build/md.elf'
  p=subprocess.run([sys.executable,__file__,tag,str(index)],env=dict(os.environ,MD_ELF=str(elf)),capture_output=True,text=True,check=True);one[tag]=json.loads(p.stdout)
 one['relative_gains_percent']=[round((b['guest_per_GBA_frame']/a['guest_per_GBA_frame']-1)*100,2) for a,b in zip(one['before'],one['after'])];rows.append(one)
 print(index,[(round(a['guest_per_GBA_frame'],3),round(b['guest_per_GBA_frame'],3),gain) for a,b,gain in zip(one['before'],one['after'],one['relative_gains_percent'])],flush=True)
(V/'FIXED_GUEST_SPEED.json').write_text(json.dumps({'guest_frames_per_window':300,'pad_script':'START 180..183; B 400..403; START 500..503; A after 600, 4 frames/80','song_command':'0x81 at guest 150','rows':rows},indent=2)+'\n')
