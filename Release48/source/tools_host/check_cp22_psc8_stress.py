#!/usr/bin/env python3
from pathlib import Path
import sys,subprocess,json,struct
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp22'
if len(sys.argv)>2:
 tag,name=sys.argv[1:3];CASES={'PS1':(0x082d3b80,0x80000,10),'Adventure':(137706368,131072,1),'Gaiden':(166669648,262144,17)}
 sys.path.insert(0,str(R/'source/tools_host/cp08_mgba'));from mgba2 import GBA
 g=GBA(R/f'binaries/RUN_THIS_R48_{"CP21_ADVENTURE_NATIVE_DIV20" if tag=="before" else "CP22_NATIVE_SPEED"}.gba',entry=0x086ca500)
 g.put(0x0203ff00,struct.pack('<IIII',*CASES[name],0));st=1;t=0;prev=0;windows=[]
 choices=[0,0,64,128,32,16,1,2,64,128,32,16,1,1,2,0]
 while t<12000:
  st=(st*1103515245+12345)&0xffffffff;r=st>>16;n=4+r%24;key=8 if r%23==0 else choices[(r>>5)%16]
  while n and t<12000:
   step=min(n,12000-t,1000-t%1000);g.run(step,key);t+=step;n-=step
   if t%1000==0:
    guest=g.rd32(0x0203e004);windows.append((guest-prev)/1000);prev=guest
    if t in [4000,8000,12000]:g.shot(V/f'{tag}_{name}_stress_{t}.png')
 print(json.dumps({'game':name,'display_frames':t,'guest_frames':prev,'rate':prev/t,'windows':windows}));sys.exit(0)
tag=sys.argv[1] if len(sys.argv)>1 else 'before';rows=[]
for name in ['PS1','Adventure','Gaiden']:
 p=subprocess.run([sys.executable,__file__,tag,name],capture_output=True,text=True,check=True);row=json.loads(p.stdout);rows.append(row)
 print(name,round(row['rate'],3),'windows',[round(x,3) for x in row['windows']],flush=True)
(V/f'PSC8_STRESS_{tag}.json').write_text(json.dumps(rows,indent=2)+'\n')
