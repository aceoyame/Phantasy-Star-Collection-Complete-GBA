import sys
from gba import *
def boot_to_debug(rom, out_state):
    g=GBA(rom,entry=0x08800000)
    g.run(120); g.press('START',3,200)
    for i in range(8): g.press('START',3,90)
    for i in range(18): g.press('A',3,40)
    g.run(60)
    for i in range(26): g.press('A',3,40)
    for i in range(30): g.press('A',3,60)
    g.run(1200)
    for i in range(15): g.press('A',3,60)
    g.save(out_state.replace('.bin','_field.bin'))
    g.run(60); g.press('START',1,60); g.press('B',1,60); g.run(30); g.press('START',1,60)
    for i in range(4): g.press('DOWN',1,20)
    g.press('A',1,60)
    g.save(out_state.replace('.bin','_menu.bin'))
    for i in range(7): g.press('DOWN',1,20)
    g.press('A',1,60)
    g.save(out_state); g.shot(out_state.replace('.bin','.png'))
    return g
if __name__=='__main__':
    boot_to_debug(sys.argv[1], sys.argv[2])
