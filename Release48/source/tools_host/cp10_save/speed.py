import sys; from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'cp09_prof'))
from gtest import *
rom=sys.argv[1]; idx=int(sys.argv[2]); cmd=int(sys.argv[3],0)
g=boot(rom,idx); zram=sym('md_zram')
for f in range(3000):
    if g.rd32(0x0203e000+4)>=150: break
    g.run(1)
def rate(n=600):
    a=g.rd32(0x0203e000+4); g.run(n); b=g.rd32(0x0203e000+4); return (b-a)/n
print('idle rate guest/GBA frame (no song): %.3f'%rate())
g.wr8(zram+0x1c0a, cmd); g.run(240)
print('song playing rate: %.3f'%rate())
