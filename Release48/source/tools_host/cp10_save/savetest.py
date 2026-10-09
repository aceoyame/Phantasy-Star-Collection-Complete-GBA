import sys, os
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'cp09_prof'))
import gtest
from gtest import boot, L
rom=sys.argv[1]; idx=int(sys.argv[2]); fr=int(sys.argv[3])
MD_RAM=gtest.sym('md_ram')
def gb(g,a):  # 68000 RAM byte at 0xFF0000|a
    return g.rd8(MD_RAM+((a&0xFFFF)^1))
def gw(g,a): return (gb(g,a)<<8)|gb(g,a+1)
def state(g):
    return dict(ffe8=hex(gw(g,0xFFE8)),fa20=gw(g,0xFA20),fa22=hex((gw(g,0xFA22)<<16)|gw(g,0xFA24)),shadow=[hex(gw(g,0xF960+2*i)) for i in range(4)])
g=boot(rom,idx)
g.run(fr)
print('run1',state(g))
sram=bytes(g.mem(0x0E000000,0x8000))
print('SRAM magic',sram[0x7F00:0x7F08], 'slot', idx-2, sram[0x7000+(idx-2)*128:0x7000+(idx-2)*128+16].hex())
changed=[i for i in range(0x8000) if sram[i]!=0xFF]; print('non-FF SRAM bytes',len(changed),hex(min(changed)) if changed else None,hex(max(changed)) if changed else None)
(Path.cwd()/('cp10_save_slot_%d.bin'%idx)).write_bytes(sram)
# power cycle with the saved SRAM
g2=gtest.GBA(rom,entry=0x08000000)
g2.put(0x0E000000,sram)
g2.run(120)
L.mg_break(0x080005f0); g2.run(10,'START'); g2.run(900); assert L.mg_hit(); L.mg_break(0xffffffff); g2.run(50)
for _ in range(4): g2.press('DOWN',1,60)
L.mg_break(0x08005000); g2.run(10,'A'); g2.run(500); assert L.mg_hit(); L.mg_break(0xffffffff); g2.run(80)
for _ in range(idx): g2.press('DOWN',1,8)
L.mg_break(0x086e2000); g2.press('A',5,0); assert L.mg_hit(); L.mg_break(0xffffffff)
g2.run(fr)
print('run2',state(g2))
