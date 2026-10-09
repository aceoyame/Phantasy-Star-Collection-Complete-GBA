import sys, subprocess, collections, bisect
import os; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from gtest import *
# prof.py ROM IDX CMD STEPS : cycle profile by function while a song plays
rom=sys.argv[1]; idx=int(sys.argv[2]); cmd=int(sys.argv[3],0); steps=int(sys.argv[4])
g=boot(rom,idx)
zram=sym('md_zram')
for f in range(2000):
    if g.rd32(0x0203e000+4)>=150: break
    g.run(1)
g.wr8(zram+0x1c0a, cmd); g.run(240)
c0=g.cycles(); g.pprof(steps); c1=g.cycles()
h=g.pprof_dump()
syms=[]
for line in subprocess.run(['llvm-nm','-n','--defined-only',ELF],capture_output=True,text=True).stdout.splitlines():
    p=line.split()
    if len(p)==3 and p[1] in 'tTwW': syms.append((int(p[0],16)&~1,p[2]))
syms.sort(); addrs=[a for a,_ in syms]
by=collections.Counter(); tot=0
for a,c in h:
    i=bisect.bisect_right(addrs,a)-1
    by[syms[i][1] if i>=0 else '?']+=c; tot+=c
print('cycles profiled',tot,'gba frames %.1f'%(tot/280896))
for n,c in by.most_common(40): print('%-32s %6.2f%%  %8.0f/frame'%(n,100*c/tot,c/(tot/280896)))
