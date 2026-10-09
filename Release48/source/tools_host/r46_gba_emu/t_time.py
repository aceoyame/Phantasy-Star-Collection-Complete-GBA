import sys
from gtest import *
ROM='/home/claude/w/test_combo.gba'
eid=int(sys.argv[1],16)
g=gba_enemy_battle(ROM,'/home/claude/w/emu/tc_debug_menu.bin',eid)
for sk in sys.argv[2:]:
    defend_round(g)
    force_skill(g,int(sk,16))
    a=sym('r46_clip_play_ex')
    if not run_until_break(g,a,900): print(sk,'no clip'); continue
    f0=g.frame(); lr=g.reg(14)
    run_until_break(g,lr&~1,3000); print(sk,'clip frames',g.frame()-f0)
    g.run(200)
