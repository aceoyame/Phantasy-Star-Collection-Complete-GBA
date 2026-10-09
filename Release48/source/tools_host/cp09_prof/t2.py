import sys; import os; sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from gtest import *
# usage: t2.py ROM IDX GBAFRAMES CMD [GUEST_FRAME_TO_INJECT]   per-frame sampling of Z80 and synth ticks (GBA cycles)
rom=sys.argv[1]; idx=int(sys.argv[2]); frames=int(sys.argv[3]); cmd=int(sys.argv[4],0) if len(sys.argv)>4 else 0
gfi=int(sys.argv[5]) if len(sys.argv)>5 else 150
g=boot(rom,idx)
zram=sym('md_zram'); st=sym('snd_stats')
g.pcm('/tmp/claude-0/sp/gt/out.pcm')
S=[]; inj=None
def rdst(): return [g.rd32(st+4*k) for k in range(10)]
for f in range(frames):
    hb=[g.rd32(0x0203e000+4*k) for k in (1,13,24,26,27,28,30)]
    if cmd and inj is None and hb[0]>=gfi:
        g.wr8(zram+0x1c0a if cmd<0x90 else zram+0x1c0b, cmd); inj=(f,rdst())
    g.run(1)
    S.append(hb)
g.pcm('')
import statistics
def stat(name,col,lo,hi):
    v=[s[col] for s in S[lo:hi]]
    if not v: return
    v2=sorted(v)
    print('%-10s mean %8.0f median %8.0f p90 %8.0f max %8d'%(name,statistics.mean(v),v2[len(v2)//2],v2[int(len(v2)*.9)],v2[-1]))
lo=inj[0]+200 if inj else 0
print('injected at gba frame',inj[0] if inj else None,'guest frame at end',S[-1][0],'late',S[-1][3],'audio frames',S[-1][4])
stat('snd total',2,lo,frames); stat('z80',5,lo,frames); stat('synth+mix',6,lo,frames)
now=rdst()
if inj:
    d=[a-b for a,b in zip(now,inj[1])]
    print('since inject: keyons %d slices %d skips %d events %d  texec %d tskip %d z80frames %d  => per frame: slices %.1f texec %.0f tskip %.0f events %.0f'%(d[0],d[2],d[3],d[4],d[7],d[8],d[9],d[2]/d[9],d[7]/d[9],d[8]/d[9],d[4]/d[9]))
raw=open('/tmp/claude-0/sp/gt/out.pcm','rb').read()
wav('/tmp/claude-0/sp/gt/out.wav',raw)
print('rms per second',[round(x) for x in rms_blocks(raw)][:14])
