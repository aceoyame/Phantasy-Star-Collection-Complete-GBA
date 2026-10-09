import sys, subprocess, collections, bisect, re
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'cp09_prof'))
from gtest import *
rom=sys.argv[1]; idx=int(sys.argv[2]); cmd=int(sys.argv[3],0); steps=int(sys.argv[4]); fn=sys.argv[5]
g=boot(rom,idx); zram=sym('md_zram')
for f in range(3000):
    if g.rd32(0x0203e000+4)>=150: break
    g.run(1)
g.wr8(zram+0x1c0a, cmd); g.run(240)
g.pprof(steps); h=g.pprof_dump()
lo=sym(fn)&~1
nm=subprocess.run(['llvm-nm','-n','-S','--defined-only',ELF],capture_output=True,text=True).stdout.splitlines()
size=0
for l in nm:
    p=l.split()
    if len(p)==4 and p[3]==fn: size=int(p[1],16)
tot=sum(c for a,c in h); inn=[(a,c) for a,c in h if lo<=a<lo+size]
print(fn,'size',size,'share %.1f%%'%(100*sum(c for a,c in inn)/tot))
lines=collections.Counter()
addrs=[hex(a) for a,c in inn]
out=subprocess.run(['llvm-addr2line','-e',ELF,'-f','-i']+addrs,capture_output=True,text=True).stdout.splitlines()
# with -f -i output has per address: function/file:line pairs, inline chain; take innermost (first pair)
# parse: each address yields sequences of (func, loc) until next address; use separator by counting? fall back: query one by one for top 40
inn.sort(key=lambda x:-x[1])
out=subprocess.run(['llvm-addr2line','-e',ELF,'-f','-s']+[hex(a) for a,c in inn],capture_output=True,text=True).stdout.splitlines()
byf=collections.Counter()
for k,(a,c) in enumerate(inn): byf[out[2*k]]+=c
print('by inlined function (share of total):')
for n,c in byf.most_common(14): print('  %-18s %5.2f%%  %7.0f/frame'%(n,100*c/tot,c*280896/tot/0.518))
sys.exit()
for a,c in inn[:45]:
    r=subprocess.run(['llvm-addr2line','-e',ELF,'-f','-s',hex(a)],capture_output=True,text=True).stdout.split('\n')
    print('%8x %5.2f%% %s %s'%(a,100*c/tot,r[0],r[1]))
