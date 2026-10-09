#!/usr/bin/env python3
"""Native DblSlash one/left/two-claw visuals and once-only skill consumption.

Requires the matching-ROM state produced by physical_range_emulator first.
This tests current native gameplay, not source two-hit arithmetic equivalence.
"""
from pathlib import Path
import ctypes,json,sys
from PIL import Image
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
def mem(g,a,n):return bytes(g.rd8(a+i) for i in range(n))
def put(g,a,b):
 for i,v in enumerate(b):g.wr8(a+i,v)
def call(g,name,args=()):
 for reg,val in enumerate(args[:4]):L.gba_set_reg(reg,val)
 for i,val in enumerate(args[4:]):g.wr32(0x03007d80+i*4,val)
 for reg,val in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(reg,val)
 assert run_until_break(g,0x08800000,20),name
results=[]
for tag,right,left in [('ONE_CLAW',0x20,0),('LEFT_CLAW',0,0x20),('TWO_CLAWS',0x20,0x24)]:
 g=GBA(str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba'),entry=0x08800000)
 g.load(str(O/'R46_CP09_DF3_MENU.bin'))
 pp,ee,cp,tp=0x0200dd60,0x0200dea0,0x0200dfb0,0x0200dfe0
 call(g,'battle_load_char',(5,pp));p=bytearray(mem(g,pp,62));p[:4]=(999).to_bytes(2,'little')*2
 p[8:10]=(300).to_bytes(2,'little');p[25:29]=bytes([255,255,0,0]);put(g,pp,p+bytes(248))
 d=0x0200f500+5*128;put(g,d+0x4c,bytes([right,left]));put(g,d+0x62,bytes([3])+bytes(7));put(g,d+0x6a,bytes([3,3])+bytes(14))
 for i in range(4):
  call(g,'battle_load_enemy',(9,ee+i*62));g.wr16(ee+i*62,999);g.wr16(ee+i*62+2,999)
  g.wr8(ee+i*62+25,0);g.wr8(ee+i*62+27,0)
  for j in range(14):g.wr8(ee+i*62+34+j,4)
  g.wr8(s['battle_enemy_placement']+i,(8,18,28,38)[i])
 g.wr32(s['battle_enemy_count'],4);g.wr8(s['battle_event_index'],1)
 call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,1,ee,2))
 put(g,s['battle_enemy_reaction_flags'],bytes(4))
 call(g,'battle_compose_igg',(pp,1,ee,2,0));call(g,'battle_present_page0')
 before=mem(g,ee,248);uses=mem(g,d+0x6a,16);inv=mem(g,0x0200f410,40)
 put(g,cp,bytes([3,3,2,255])+bytes(16));g.wr32(tp,2)
 g.wr32(0x03007d80,0xffffffff);g.wr32(0x03007d84,tp)
 for reg,val in [(0,pp),(1,1),(2,ee),(3,cp),(13,0x03007d80),(14,0x08800000),(15,s['battle_resolve_mixed_round'])]:L.gba_set_reg(reg,val)
 assert run_until_break(g,s['battle_party_source_weapon_skill'],80)
 frames=[];display_counts=[]
 for phase in range(2):
  assert run_until_break(g,s['battle_party_source_attack'],20),'Missing DblSlash pass '+str(phase)
  lr=g.reg(14);assert mem(g,ee,248)==before and mem(g,d+0x6a,16)==uses
  L.gba_set_break(ctypes.c_uint32(lr&~1));f0=g.frame()
  for tick in range(400):
   g.run(1)
   if tick and tick%8==0:frames.append(g.shot())
   if L.gba_break_hit():break
  assert L.gba_break_hit(),'Weapon pass did not return'
  display_counts.append(g.frame()-f0)
  assert mem(g,ee,248)==before and mem(g,pp,62)==p and mem(g,d+0x6a,16)==uses
 assert run_until_break(g,s['battle_execute_cmd'],20)
 lr=g.reg(14);assert run_until_break(g,lr&~1,20)
 after=[g.rd16(ee+i*62) for i in range(4)];assert after[2]<999 and all(after[i]==999 for i in (0,1,3))
 assert mem(g,d+0x6a,16)==bytes([2,3])+bytes(14),'Skill consumed more than once'
 assert mem(g,0x0200f410,40)==inv and mem(g,pp,62)==p
 assert run_until_break(g,s['battle_finish_damage'],20);lr=g.reg(14)
 assert not g.rd8(s['battle_suppress_skill_source_audio'])
 assert run_until_break(g,lr&~1,300)
 assert [g.rd16(ee+i*62) for i in range(4)]==after
 montage=Image.new('RGB',(720,320))
 for i in range(6):montage.paste(frames[min(len(frames)-1,i*(len(frames)-1)//5)],((i%3)*240,(i//3)*160))
 montage.save(O/('R46_CP09_DBLSLASH_'+tag+'.png'))
 results.append({'case':tag,'hands':[right,left],'visual_passes':2,'custom_core_display_counts':display_counts,'hp_after':after,'visual_preserves_hp_actors_resources':True,'one_skill_use_and_gameplay_commit':True})
 print('PASS native DblSlash',tag,'two visuals / one use',flush=True)
(O/'R46_WEAPON_SKILLS_EMULATOR_EVIDENCE.json').write_text(json.dumps({'result':'PASS','cases':results,'scope':'Actual native mixed-round/weapon-skill/attack/gameplay/damage path; debug stats, skill slots, commands and matching-ROM state are fixtures. Custom core is HLE/non-cycle-accurate/no audio. DblSlash source two-hit arithmetic and hit/miss/window timing, Disrupt overlap and full skills remain open.'},indent=2)+'\n')
