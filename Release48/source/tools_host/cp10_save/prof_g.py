import sys, subprocess, collections, bisect, re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'cp09_prof'))
from gtest import *
rom=sys.argv[1]; idx=int(sys.argv[2]); cmd=int(sys.argv[3],0); steps=int(sys.argv[4])
g=boot(rom,idx); zram=sym('md_zram')
for f in range(3000):
    if g.rd32(0x0203e000+4)>=150: break
    g.run(1)
g.wr8(zram+0x1c0a, cmd); g.run(240)
f0=g.rd32(0x0203e000+4); g.pprof(steps); g.pprof(steps) if False else None
h=g.pprof_dump()
f1=g.rd32(0x0203e000+4)
syms=[]
for line in subprocess.run(['llvm-nm','-n','--defined-only',ELF],capture_output=True,text=True).stdout.splitlines():
    p=line.split()
    if len(p)==3 and p[1] in 'tTwW': syms.append((int(p[0],16)&~1,p[2]))
syms.sort(); addrs=[a for a,_ in syms]
by=collections.Counter(); tot=0
for a,c in h:
    i=bisect.bisect_right(addrs,a)-1
    by[syms[i][1] if i>=0 else '?']+=c; tot+=c
print('profiled cycles',tot,'guest frames in window',f1-f0)
grp=collections.Counter()
def G(n):
    if re.match(r'(mdsnd|z80|snd_|ch_|awr|ard|ym_|psg|svoice|timers|idle_skip|render_segment)',n): return 'sound'
    if re.match(r'(vdp_|bus_|render|mdr_|io_|dma|ee_|md_run|md_present|md_main|md_wait|md_begin)',n) or 'render_line' in n: return 'vdp/bus/frame'
    return 'cpu(68000 engine+AOT)'
for n,c in by.items(): grp[G(n)]+=c
fr=max(1,f1-f0)
for k,c in grp.most_common(): print('%-24s %6.1f%%  %9.0f GBA cycles per guest frame'%(k,100*c/tot,c/fr))
print('total per guest frame %.0f'%(tot/fr))
for n,c in by.most_common(60): print('%-34s %6.2f%% %9.0f/guest frame  [%s]'%(n,100*c/tot,c/fr,G(n)))
