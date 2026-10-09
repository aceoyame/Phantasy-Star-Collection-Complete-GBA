import sys
from gtest import *
from battle import capture
ROM='/home/claude/w/R45/binaries/RUN_THIS_R46_PSC.gba'
fid=int(sys.argv[1]); tag=sys.argv[2]
g=gba_boss_battle(ROM,'/home/claude/w/emu/r46_debug.bin',fid,settle=10)
ps=capture(g,600,12,tag)
montage(ps,'shots/%s_m.png'%tag,cols=10)
g.save('st_%s.bin'%tag)
