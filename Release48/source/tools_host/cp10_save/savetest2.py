import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'cp09_prof'))
import gtest
from gtest import boot, L
rom=sys.argv[1]; idx=int(sys.argv[2]); fr=int(sys.argv[3])
MD_RAM=gtest.sym('md_ram')
gb=lambda g,a: g.rd8(MD_RAM+((a&0xFFFF)^1))
gw=lambda g,a: (gb(g,a)<<8)|gb(g,a+1)
def bootsram(sram):
    g=gtest.GBA(rom,entry=0x08000000)
    if sram: g.put(0x0E000000,sram)
    g.run(120)
    L.mg_break(0x080005f0); g.run(10,'START'); g.run(900); assert L.mg_hit(); L.mg_break(0xffffffff); g.run(50)
    for _ in range(4): g.press('DOWN',1,60)
    L.mg_break(0x08005000); g.run(10,'A'); g.run(500); assert L.mg_hit(); L.mg_break(0xffffffff); g.run(80)
    for _ in range(idx): g.press('DOWN',1,8)
    L.mg_break(0x086e2000); g.press('A',5,0); assert L.mg_hit(); L.mg_break(0xffffffff)
    return g
g=bootsram(None); g.run(fr)
sram=bytearray(g.mem(0x0E000000,0x8000))
base=0x7000+(idx-2)*128
img=sram[base:base+128]
print('run1: fa20=%d, SRAM slot first bytes %s'%(gw(g,0xFA20),bytes(img[:16]).hex()))
# patch word 2 (bytes 4,5) of checksummed block 1 (words 1..6 -> bytes 2..13, checksum in byte 13)
img[4]=0x12; img[5]=0x34
img[13]=(sum(img[2:13])+0x5a)&0xFF
sram[base:base+128]=img
g2=bootsram(bytes(sram)); g2.run(fr)
print('run2 (patched save loaded): shadow word2=%04x (expect 1234) fa20=%d'%(gw(g2,0xF960+4),gw(g2,0xFA20)))
after=bytes(g2.mem(0x0E000000,0x8000))
print('SRAM unchanged by run2:', after[base:base+128]==bytes(img), ' magic', after[0x7F00:0x7F05])
