#!/usr/bin/env python3
"""Approximate native cycle costs from a fixed guest music timeline."""
import bisect,collections,subprocess,sys
from pathlib import Path
R=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(R/'source/tools_host/cp09_prof'))
from gtest import boot,sym,ELF
g=boot(R/'binaries/RUN_THIS_R48_CP13_TEST.gba',2)
while g.rd32(0x0203e004)<150:g.run(1)
g.wr8(sym('md_zram')+0x1c0a,0x81)
while g.rd32(0x0203e004)<320:g.run(1)
first=g.rd32(0x0203e004);g.pprof(5000000);last=g.rd32(0x0203e004)
syms=[]
for line in subprocess.run(['llvm-nm','-n','--defined-only',ELF],capture_output=True,text=True,check=True).stdout.splitlines():
 p=line.split()
 if len(p)==3 and p[1] in 'tTwW':syms.append((int(p[0],16)&~1,p[2]))
syms.sort();addrs=[a for a,_ in syms];by=collections.Counter();total=0
for address,cycles in g.pprof_dump():
 i=bisect.bisect_right(addrs,address)-1
 by[syms[i][1] if i>=0 else '?']+=cycles;total+=cycles
lines=[f'Fixed song command 81, guest frame {first} through {last}; approximately {round(total/(last-first))} cycles/guest frame.']
for name,cycles in by.most_common(25):lines.append(f'{name:36s} {cycles*100/total:5.2f}% {round(cycles/(last-first)):8d} cycles/guest frame')
out='\n'.join(lines)+'\n';(R.parent/'validation_cp13/PROFILE_FINAL.txt').write_text(out);print(out,end='')
