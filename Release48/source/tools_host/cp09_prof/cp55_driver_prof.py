import sys, subprocess, collections
import os; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from gtest import *
rom=sys.argv[1]; fn=sys.argv[2]
g=boot(rom,3); zram=sym('md_zram')
for f in range(2000):
    if g.rd32(0x0203e000+4)>=150: break
    g.run(1)
g.wr8(zram+0x1c0a, 0x81); g.run(240)
g.pprof(2000000); h=dict(g.pprof_dump())
lo=sym(fn)&~1
nm=subprocess.run(['llvm-nm','-n','-S',ELF],capture_output=True,text=True).stdout.split('\n')
size=[int(l.split()[1],16) for l in nm if l.endswith(' '+fn)][0]
tot=sum(h.values())
rows=sorted(((c,a) for a,c in h.items() if lo<=a<lo+size),reverse=True)[:40]
print('fn total %.1f%%'%(100*sum(c for c,a in rows)/tot))
for c,a in rows: print(hex(a), '%5.2f%%'%(100*c/tot))
