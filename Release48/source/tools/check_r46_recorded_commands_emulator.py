#!/usr/bin/env python3
"""Actual native turn dispatch on supplied non-cycle-accurate/HLE GBA core.

Debug actor/command fixtures enter battle_resolve_mixed_round, stop after the
first actor's damage step. No ROM hook is shipped. Not a full vehicle battle,
real-device timing, audio-output or normal-save integration certification.
"""
from pathlib import Path
import ctypes,json,sys
from PIL import Image
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
from boot_debug import boot_to_debug
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
def mem(g,a,n):return bytes(g.rd8(a+i) for i in range(n))
def put(g,a,b):
 for i,v in enumerate(b):g.wr8(a+i,v)
def native_call(g,name,args=()):
 for reg,val in enumerate(args):L.gba_set_reg(reg,val)
 for reg,val in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(reg,val)
 assert run_until_break(g,0x08800000,10),name+' did not return'
 return g.reg(0)
g=boot_to_debug(rom,str(O/'R46_CP06_DEBUG.bin'))
for _ in range(3):g.press('RIGHT',3,20)
g.press('A',3,300);g.run(120)
assert g.rd8(g.rd32(s['battle_ui_enemies'])+20)==0x84,'Wrong debug boss'
state=O/'R46_CP06_DF3_MENU.bin';g.save(str(state));g.shot(str(O/'R46_CP06_DF3_START.png'),scale=2)
results=[]
for kind,ident,skill,tag in [(10,1,0,'LAND_ROVER'),(10,2,0,'ICE_DIGGER'),(10,3,0,'HYDROFOIL'),(10,2,7,'VEHICLE_NSPHER'),(9,0x8b,0,'DYNAMITE'),(9,0x8e,0,'ECLIPSE_TORCH')]:
 g=GBA(rom,entry=0x08800000);g.load(str(state))
 pp,ee,cp,tp=0x0200dd60,0x0200dea0,0x0200dfb0,0x0200dfe0
 party=bytearray(mem(g,g.rd32(s['battle_ui_party']),310));enemy=bytearray(mem(g,g.rd32(s['battle_ui_enemies']),248));put(g,pp,party)
 if kind==10:
  put(g,0x0200f43c,bytes([0,ident]));native_call(g,'battle_fill_stats');assert native_call(g,'battle_live_party',(pp,))==1
 else:native_call(g,'battle_load_char',(0,pp))
 party=bytearray(mem(g,pp,310));party[25]=255;party[26]=255;party[27]=0;party[28]=0
 for i in range(1,5):party[i*62:i*62+2]=bytes(2)
 enemy[:2]=(32767).to_bytes(2,'little');enemy[25]=0;enemy[27]=0
 for i in range(1,4):enemy[i*62:i*62+2]=bytes(2)
 put(g,pp,party);put(g,ee,enemy);g.wr32(s['battle_ui_party'],pp);g.wr32(s['battle_ui_enemies'],ee);g.wr32(s['battle_ui_party_count'],1);g.wr32(s['battle_enemy_count'],1)
 g.wr8(s['battle_palette_valid'],0);g.wr8(s['battle_palette_hw_dirty'],1)
 command=bytes([4 if kind==9 else 6 if skill else 1,ident if kind==9 else skill,0,0]);put(g,cp,command+bytes(16));g.wr32(tp,0)
 put(g,0x0200f410,bytes([ident,ident])+bytes(38));put(g,0x0200476a,bytes([8])*16)
 before_inv=mem(g,0x0200f410,40);before_uses=mem(g,0x0200476a,16);g.wr8(s['battle_suppress_skill_source_audio'],0)
 # Real native queue, with first party actor guaranteed faster in this fixture.
 g.wr32(0x03007d80,0xffffffff);g.wr32(0x03007d84,tp)
 for reg,val in [(0,pp),(1,1),(2,ee),(3,cp),(13,0x03007d80),(14,0x08800000),(15,s['battle_resolve_mixed_round'])]:L.gba_set_reg(reg,val)
 assert run_until_break(g,s['r46_clip_play'],400),'Native command did not select captured playback'
 lr=g.reg(14);L.gba_set_break(ctypes.c_uint32(lr&~1));frames=[];f0=g.frame()
 for tick in range(1200):
  g.run(1)
  if tick%5==0:frames.append(g.shot())
  if L.gba_break_hit():break
 assert L.gba_break_hit(),'Command visual did not return'
 display=g.frame()-f0
 assert mem(g,pp,310)==party and mem(g,ee,248)==enemy,'Visual changed actors'
 assert mem(g,0x0200f410,40)==before_inv and mem(g,0x0200476a,16)==before_uses
 assert run_until_break(g,s['battle_execute_cmd'],30),'Native gameplay not reached'
 assert (g.reg(0),g.reg(1),g.reg(2),g.reg(3))==(pp,1,ee,0),'Wrong gameplay actor'
 assert g.rd8(s['battle_suppress_skill_source_audio'])==1,'Duplicate native cue not suppressed'
 lr=g.reg(14);assert run_until_break(g,lr&~1,30),'Gameplay did not return'
 if kind==9:
  assert mem(g,0x0200f410,40)==bytes([ident])+bytes(39),'Item was not consumed exactly once'
  assert mem(g,0x0200476a,16)==before_uses
 elif skill:
  want=bytearray(before_uses);want[(skill-1)*2]=7;assert mem(g,0x0200476a,16)==want,'Skill use was not consumed exactly once'
  assert mem(g,0x0200f410,40)==before_inv
 else:assert mem(g,0x0200f410,40)==before_inv and mem(g,0x0200476a,16)==before_uses
 assert run_until_break(g,s['battle_finish_damage'],30),'Native damage step not reached'
 assert g.rd8(s['battle_suppress_skill_source_audio'])==0,'Suppression leaked into next action'
 out=Image.new('RGB',(720,320))
 for i in range(6):out.paste(frames[min(len(frames)-1,i*(len(frames)-1)//5)],((i%3)*240,(i//3)*160))
 out.save(O/('R46_CP06_'+tag+'.png'))
 results.append({'case':tag,'route':'native mixed-round party dispatch through captured visual, gameplay and damage step','display_frames':display,'visual_actors_resources_unchanged':True,'resource_execution_once':True,'audio_suppression_restored':True,'enemy_hp_after':g.rd16(ee)})
 print('PASS',tag,display,'display frames, native gameplay/resources once',flush=True)
(O/'R46_RECORDED_COMMANDS_EMULATOR_EVIDENCE.json').write_text(json.dumps({'core':'Supplied custom GBA; non-cycle-accurate/HLE/no audio output','debug_fixtures':'One fast actor and one living DF3 enemy; manual command/vehicle identity; stop at first damage step','cases':results,'hardware_or_full_vehicle_battle_certified':False},indent=2)+'\n')
