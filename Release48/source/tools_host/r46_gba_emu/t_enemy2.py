import sys
from gtest import *
from battle import capture
import os; ROM=os.environ.get('TROM','/home/claude/w/R45/binaries/RUN_THIS_R46_PSC.gba')
eid=int(sys.argv[1],16); sk=int(sys.argv[2],16); tag=sys.argv[3]
g=gba_enemy_battle(ROM,os.environ.get('TMENU','/home/claude/w/emu/r46_debug_menu.bin'),eid)
defend_round(g)
print('forced',force_skill(g,sk))
ps=capture(g,int(os.environ.get('TF','240')),int(os.environ.get('TE','8')),tag)
from PIL import Image
W=Image.new('RGB',(240*6,160*5))
for i,p in enumerate(ps): W.paste(Image.open(p),((i%6)*240,(i//6)*160))
W.save('/tmp/claude_%s.png'%tag)
