#!/usr/bin/env python3
"""Screenshots from the supplied non-cycle-accurate GBA core.

The two DF3 cases use native battle commands with a forced debug AI return.
Combo cases invoke the linked visual player in an existing battle; they do
not certify combo formation/resource handling or source recording quality.
No test hook is written into the shipped ROM.
"""
from pathlib import Path
import sys,re,struct,ctypes,json
from PIL import Image
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
ROM=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
symbols={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:symbols[p[-1]]=int(p[0],16)
  except ValueError:pass
def mem(g,a,n):return bytes(g.rd8(a+i) for i in range(n))
def put(g,a,b):
 for i,v in enumerate(b):g.wr8(a+i,v)
def start():
 g=GBA(ROM,entry=0x08800000);g.load(str(O/'R46_CP06_DF3_MENU.bin'))
 e=g.rd32(symbols['battle_ui_enemies']);assert g.rd8(e+20)==0x84
 return g
def montage(frames,name):
 # Six evenly distributed source-display snapshots; original-size pixel QA.
 selected=[frames[min(len(frames)-1,i*(len(frames)-1)//5)] for i in range(6)]
 out=Image.new('RGB',(720,320))
 for i,im in enumerate(selected):out.paste(im,((i%3)*240,(i//3)*160))
 out.save(O/(name+'.png'))
 selected[3].resize((480,320),Image.Resampling.NEAREST).save(O/(name+'_detail.png'))
report=[]
for skill in [0x4b,0x51]:
 g=start();entry=symbols['battle_enemy_pick_skill'];L.gba_set_break(ctypes.c_uint32(entry));g.press('A',3,30)
 for _ in range(5):
  for _ in range(4):g.press('RIGHT',3,8)
  g.press('A',3,25)
 if not L.gba_break_hit():assert run_until_break(g,entry,900),'native enemy AI entry not reached'
 L.gba_set_break(ctypes.c_uint32(0xffffffff));lr=g.reg(14);assert run_until_break(g,lr&~1,5)
 L.gba_set_reg(0,skill)
 assert run_until_break(g,symbols['r46_clip_play'],900),'selected native dispatch not reached'
 p=g.rd32(symbols['battle_ui_party']);e=g.rd32(symbols['battle_ui_enemies']);ph=mem(g,p,310);eh=mem(g,e,248)
 lr=g.reg(14);L.gba_set_break(ctypes.c_uint32(lr&~1));frames=[];f0=g.frame()
 for i in range(1500):
  g.run(1)
  if i%4==0:frames.append(g.shot())
  if L.gba_break_hit():break
 assert L.gba_break_hit(),'native selected playback did not return'
 assert mem(g,p,310)==ph and mem(g,e,248)==eh,'visual playback changed actor data'
 L.gba_set_break(ctypes.c_uint32(0xffffffff));g.run(90)
 tag='R46_CP06_DF3_'+format(skill,'02X');montage(frames,tag);g.shot(str(O/(tag+'_after.png')),scale=2)
 report.append({'case':tag,'route':'native battle, AI return forced for debug','display_frames':g.frame()-f0-90,'actor_data_unchanged_during_visual':True})
header=(R/'gba/r46_battle_clips.h').read_text();clip_off=int(re.search(r'#define r46_clips .*R46_TABLE_BASE\+(\d+)u',header)[1])
for ci,name in [(2,'HOLOCAUST'),(3,'BLACK_HOLE')]:
 g=start();oldp=g.rd32(symbols['battle_ui_party']);olde=g.rd32(symbols['battle_ui_enemies'])
 # Use the unused tail after R46Scratch, disjoint from the C call stack.
 pp,ee=0x0200dd60,0x0200dea0;ph=mem(g,oldp,310);eh=mem(g,olde,248);put(g,pp,ph);put(g,ee,eh)
 g.wr32(symbols['battle_ui_party'],pp);g.wr32(symbols['battle_ui_enemies'],ee)
 ex=[0x80+(g.rd8(symbols['battle_enemy_placement']+i)&127)*8 for i in range(4)]
 ctx=struct.pack('<5i5B3x4i4BiiBB2x',0x120,0xf0,0x150,0xc0,0x180,*[1]*5,*ex,1,0,0,0,0x120,ex[0],1,1)
 put(g,0x03007e80,ctx);g.wr16(0x04000000,g.rd16(0x04000000)&~0x80)
 L.gba_set_reg(0,symbols['r46_recorded_tables']+clip_off+ci*36);L.gba_set_reg(1,0x03007e80);L.gba_set_reg(2,ee);L.gba_set_reg(13,0x03007d80);L.gba_set_reg(14,0x08800000);L.gba_set_reg(15,symbols['r46_clip_play']);L.gba_set_break(ctypes.c_uint32(0x08800000))
 frames=[];f0=g.frame()
 for i in range(1800):
  g.run(1)
  if i%4==0:frames.append(g.shot())
  if L.gba_break_hit():break
 assert L.gba_break_hit(),'linked combo visual player did not return'
 assert mem(g,pp,310)==ph and mem(g,ee,248)==eh
 tag='R46_CP06_'+name;montage(frames,tag);report.append({'case':tag,'route':'linked visual-player invocation; not integrated combo gameplay','display_frames':g.frame()-f0,'actor_data_unchanged_during_visual':True})
(O/'R46_RECORDED_EMULATOR_EVIDENCE.json').write_text(json.dumps({'core':'Claude supplied headless GBA, not cycle accurate, HLE BIOS; no sound output','cases':report,'hardware_finishability_certified':False},indent=2)+'\n')
print(json.dumps(report,indent=2))
