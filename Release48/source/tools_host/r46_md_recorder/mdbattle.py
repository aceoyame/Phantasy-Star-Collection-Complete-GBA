from mdplay import *
import ctypes
def gba_party(st='/home/claude/w/emu/st_bf18.bin'):
    from gba import GBA
    g=GBA('/home/claude/w/R45_orig.gba',entry=0x08800000); g.load(st)
    ew=g.mem(0,0x40000)
    return ew[0xF40A:0xF40F], ew[0xF500:0xFA80]
_party=None
def start_battle(idx,state='/home/claude/w/md/st_field2.bin',frames=200):
    global _party
    m=MD(); m.load(state)
    if _party is None: _party=gba_party()
    slots,chars=_party
    m.setram(0xF40A,slots); m.setram(0xF500,chars)
    m.w8(0xECFC,idx)
    L.md_set_break(0x5199A)
    for _ in range(10):
        m.run(1)
        if L.md_break_hit(): break
    assert L.md_break_hit()
    L.md_set_break(0xFFFFFFFF)
    L.md_set_reg(16,0x5A608)
    m.run(frames)
    return m
def md_round_defend(m,members=5):
    m.press('C',3,30)
    for _ in range(members):
        for _ in range(4): m.press('RIGHT',3,8)
        m.press('C',3,25)
def run_until_break(m,pc,maxframes=600):
    L.md_set_break(pc)
    for _ in range(maxframes):
        m.run(1)
        if L.md_break_hit(): L.md_set_break(0xFFFFFFFF); return True
    L.md_set_break(0xFFFFFFFF); return False
def force_enemy_action(m,attack_pc,action,maxframes=900):
    if not run_until_break(m,attack_pc,maxframes): return False
    a4=L.md_reg(12); m.w16(a4+0x24,action); return True
