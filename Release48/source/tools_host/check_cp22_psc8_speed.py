#!/usr/bin/env python3
"""PSC8 baseline/candidate throughput and actual ARM cycle profiles; no hashes."""
from pathlib import Path
import sys,subprocess,os,json,struct,bisect,collections
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp22';V.mkdir(exist_ok=True)
CASES={'PS1':(0x082d3b80,0x80000,10),'Adventure':(137706368,131072,1),'Gaiden':(166669648,262144,17)}
if len(sys.argv)>2:
 tag,name=sys.argv[1:3];rom=R/f'binaries/RUN_THIS_R48_{"CP21_ADVENTURE_NATIVE_DIV20" if tag=="before" else "CP22_NATIVE_SPEED"}.gba'
 elf=R/'source/references/cp21_final/psc8.elf' if tag=='before' else R/'source/psc8/build/psc8.elf'
 sys.path.insert(0,str(R/'source/tools_host/cp08_mgba'));from mgba2 import GBA
 g=GBA(rom,entry=0x086ca500);g.put(0x0203ff00,struct.pack('<IIII',*CASES[name],0));windows=[];prev=0
 for frame in range(2400):
  keys='START' if frame%240 in range(120,124) else 'A' if frame%240 in range(200,204) else 0
  g.run(1,keys)
  if (frame+1)%600==0:
   guest=g.rd32(0x0203e004);windows.append((guest-prev)/600);prev=guest
 g.shot(V/f'{tag}_{name}_2400.png')
 result={'game':name,'display_frames':2400,'guest_frames':prev,'rate':prev/2400,'windows':windows,'late_audio_frames':g.rd32(0x0203e018)}
 if tag=='before':
  g.pprof(1500000);hist=g.pprof_dump();syms=[]
  for line in subprocess.run(['llvm-nm','-n',str(elf)],capture_output=True,text=True,check=True).stdout.splitlines():
   s=line.split()
   if len(s)==3 and s[1] in 'tTwW':syms.append((int(s[0],16),s[2]))
  syms.sort();addrs=[x[0] for x in syms];counts=collections.Counter()
  for address,cycles in hist:
   i=bisect.bisect_right(addrs,address)-1;counts[syms[i][1] if i>=0 else hex(address)]+=cycles
  total=sum(counts.values());result['profile']=[{'symbol':symbol,'cycles':count,'percent':100*count/total} for symbol,count in counts.most_common(24)]
 print(json.dumps(result));sys.exit(0)
if __name__!='__main__':raise RuntimeError('Use CASES directly; not importable')
tag=sys.argv[1] if len(sys.argv)>1 else 'before';rows=[]
for name in CASES:
 p=subprocess.run([sys.executable,__file__,tag,name],capture_output=True,text=True,check=True);row=json.loads(p.stdout);rows.append(row)
 print(name,round(row['rate'],3),'windows',[round(x,3) for x in row['windows']],flush=True)
 for item in row.get('profile',[])[:15]:print('  %5.1f%% %s'%(item['percent'],item['symbol']),flush=True)
(V/f'PSC8_SPEED_{tag}.json').write_text(json.dumps(rows,indent=2)+'\n')
