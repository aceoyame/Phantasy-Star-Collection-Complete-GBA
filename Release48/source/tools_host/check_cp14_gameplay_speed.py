#!/usr/bin/env python3
"""Repeat the same physical-key script in CP13 and the candidate on mGBA."""
from pathlib import Path
import os,sys,json,subprocess
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp14'
if len(sys.argv)>1:
 tag,index=sys.argv[1:];index=int(index)
 sys.path.insert(0,str(R/'source/tools_host/cp09_prof'));from gtest import boot,sym
 rom=R/'binaries/RUN_THIS_R48_CP13_TEST.gba' if tag=='before' else R/'binaries/RUN_THIS_R48_CP14_TEST.gba'
 g=boot(rom,index)
 while g.rd32(0x0203e004)<150:g.run(1)
 g.wr8(sym('md_zram')+0x1c0a,0x81)
 rows=[]
 for window in range(4):
  first=g.rd32(0x0203e004)
  for frame in range(600):
   f=600*window+frame
   keys=8 if 60<=f<66 else 1 if f>150 and f%80<4 else 128 if f>700 and f%240<20 else 0
   g.run(1,keys)
  last=g.rd32(0x0203e004);rows.append({'window':window,'guest_frames':last-first,'rate':(last-first)/600})
 g.shot(V/f'GAMEPLAY_{index}_{tag}.png');print(json.dumps(rows));sys.exit(0)
rows=[]
for index in [2,9]:
 one={'index':index}
 for tag in ['before','after']:
  elf=R/'source/references/cp13_final/md.elf' if tag=='before' else R/'source/additional/build/md.elf'
  p=subprocess.run([sys.executable,__file__,tag,str(index)],env=dict(os.environ,MD_ELF=str(elf)),capture_output=True,text=True,check=True)
  one[tag]=json.loads(p.stdout)
 rows.append(one)
 print(index,[(round(a['rate'],3),round(b['rate'],3)) for a,b in zip(one['before'],one['after'])],flush=True)
(V/'GAMEPLAY_SPEED.json').write_text(json.dumps({'display_frames_per_window':600,'physical_key_script':'START at 60..65; A at f>150 and f%80<4; DOWN at f>700 and f%240<20','rows':rows},indent=2)+'\n')
