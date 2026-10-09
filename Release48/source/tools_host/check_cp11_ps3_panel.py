import sys,json,math,array
from pathlib import Path
sys.path.insert(0,'source/tools_host/cp09_prof')
from gtest import *
O=Path('../validation_cp11');O.mkdir(exist_ok=True)
rom='binaries/RUN_THIS_R48_CP11_TEST.gba'
def collection():
 g=GBA(rom);g.run(120);L.mg_break(0x080005f0);g.run(10,'START');g.run(900);assert L.mg_hit();L.mg_break(0xffffffff);g.run(50);return g
g=collection()
for _ in range(4):g.press('DOWN',1,60)
g.shot(O/'ADDITIONAL_CARD_AFTER.png',2)
L.mg_break(0x08005000);g.run(5,'A');g.run(500);assert L.mg_hit();L.mg_break(0xffffffff);g.run(80);g.shot(O/'ADDITIONAL_MENU_AFTER.png',2)
g=collection()
for _ in range(2):g.press('DOWN',1,60)
g.run(10,'A+L+R');g.run(240,'L+R');g.run(5)
print('magic',hex(g.rd32(0x0203c000)),'pc',hex(g.reg(15)),flush=True)
g.shot(O/'PSIII_PANEL_MAIN.png',2)
assert g.rd32(0x0203c000)==0x44335350
for _ in range(11):g.press('DOWN',1,4)
assert g.rd32(0x0203c004)==12
g.press('A',2,120);g.shot(O/'PSIII_PANEL_SONG12.png',2)
def pcm(name,n=120):
 p=O/(name+'.raw');g.pcm(str(p));g.run(n);g.pcm('');d=array.array('h');d.frombytes(p.read_bytes());p.unlink();return math.sqrt(sum(x*x for x in d)/len(d))
rms=pcm('PSIII_SONG12');print('rms12',rms,flush=True);assert rms>100
for _ in range(6):g.press('SELECT',1,4);g.press('RIGHT',1,4)
assert g.rd32(0x0203c010)==63
g.run(60);muted=pcm('PSIII_MUTED');print('muted',muted,flush=True);assert muted<3
g.shot(O/'PSIII_PANEL_ALL_MUTED.png',2)
g.press('B',2,240);assert g.rd32(0x0203c000)!=0x44335350
print('exit pc',hex(g.reg(15)),flush=True)
g.shot(O/'PSIII_AFTER_EXIT.png',2)
(O/'PSIII_PANEL_CHECK.json').write_text(json.dumps({'song':12,'playing_rms':rms,'all_muted_rms':muted,'clean_restart':True},indent=2)+'\n')
