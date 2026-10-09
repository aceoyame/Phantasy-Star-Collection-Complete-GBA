from gba import *
from mont import montage
import ctypes, re, sys
MAP='/home/claude/w/R45/source/build-psc-module/ps4_psc_module.map'
def sym(name):
    for line in open(MAP):
        p=line.split()
        if len(p)>=5 and p[-1]==name: return int(p[0],16)
    raise KeyError(name)
def gba_boss_battle(rom,state,fid,settle=380):
    g=GBA(rom,entry=0x08800000); g.load(state)
    if fid>=16: g.press('RIGHT',3,15); n=fid-16
    else: n=fid
    for _ in range(n): g.press('DOWN',3,12)
    g.press('A',3,10); g.run(settle); return g
def defend_round(g,members=5):
    g.press('A',3,30)
    for m in range(members):
        for _ in range(4): g.press('RIGHT',3,8)
        g.press('A',3,25)
def force_skill(g,skill,maxframes=900):
    entry=sym('battle_enemy_pick_skill')
    if not run_until_break(g,entry,maxframes): return False
    lr=g.reg(14)
    if not run_until_break(g,lr&~1,5): return False
    L.gba_set_reg(0,skill); return True

def gba_enemy_battle(rom,menu_state,eid,settle=420):
    g=GBA(rom,entry=0x08800000); g.load(menu_state)
    for _ in range(2): g.press('DOWN',1,20)
    g.press('A',1,60)
    cur=9
    while cur+16<=eid: g.press('RIGHT',3,12); cur+=16
    while cur>eid: g.press('UP',3,12); cur-=1
    while cur<eid: g.press('DOWN',3,12); cur+=1
    g.press('A',3,10); g.run(settle); return g
