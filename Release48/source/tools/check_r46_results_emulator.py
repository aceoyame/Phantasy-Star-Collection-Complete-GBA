#!/usr/bin/env python3
"""Actual ROM result-screen/reward and debug menu escape checks, custom HLE core."""
from pathlib import Path
import argparse,ctypes,json,sys,re
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
from boot_debug import boot_to_debug
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
def mem(g,a,n):return bytes(g.rd8(a+i) for i in range(n))
def put(g,a,b):
 for i,v in enumerate(b):g.wr8(a+i,v)
def begin(g,name,args=()):
 for reg,val in enumerate(args[:4]):L.gba_set_reg(reg,val)
 for i,val in enumerate(args[4:]):g.wr32(0x03007d80+i*4,val)
 for reg,val in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(reg,val)
def call(g,name,args=()):
 begin(g,name,args);assert run_until_break(g,0x08800000,60),name
def press_to_break(g,pc,maxframes=400):
 L.gba_set_break(ctypes.c_uint32(pc))
 for tick in range(maxframes):
  g.run(1,'A' if tick<10 else 0)
  if L.gba_break_hit():
   L.gba_set_break(ctypes.c_uint32(0xffffffff));return True
 L.gba_set_break(ctypes.c_uint32(0xffffffff));return False
ap=argparse.ArgumentParser();ap.add_argument('--reuse-state',action='store_true');ap.add_argument('--prefix',default='CP10');args=ap.parse_args()
prefix='R46_'+args.prefix
state=O/(prefix+'_DF3_MENU.bin')
if not args.reuse_state:
 g=boot_to_debug(rom,str(O/(prefix+'_DEBUG.bin')))
 for _ in range(3):g.press('RIGHT',3,20)
 g.press('A',3,300);g.run(120);g.save(str(state))
md=int(re.search(r'work_ram\s*=\s*\(uint8_t\*\)\s*(0x[0-9a-fA-F]+)',(R/'gba/title_runtime_v06.c').read_text())[1],16);results=[]
def r16(g,a):return int.from_bytes(mem(g,md+a,2),'big')
def r32(g,a):return int.from_bytes(mem(g,md+a,4),'big')
def w16(g,a,v):put(g,md+a,v.to_bytes(2,'big'))
def w32(g,a,v):put(g,md+a,v.to_bytes(4,'big'))
# Exercise RUN in the real event collector, then verify that control exits
# through its caller, with no reward/inventory/story-flag mutation.
g=GBA(rom,entry=0x08800000);g.load(str(state))
assert g.rd8(s['battle_debug_battle_active'])==1
flags=mem(g,md+0xF100,96);inventory=mem(g,md+0xF410,40);money=r32(g,0xF438)
g.press('DOWN',3,20);g.press('DOWN',3,20)
assert press_to_break(g,s['battle_exit_debug_test']),'Event RUN did not select debug abort'
lr=g.reg(14);assert run_until_break(g,s['battle_ui_wait_edge'],1000),'Debug message did not reach input'
g.run(8);g.shot(str(O/(prefix+'_DEBUG_EXIT.png')),scale=2)
assert press_to_break(g,lr&~1) and g.reg(0)==1,'Debug abort did not return success'
L.gba_set_break(ctypes.c_uint32(0xffffffff));g.run(80)
assert not g.rd8(s['battle_debug_battle_active']),'Debug battle wrapper did not exit'
assert mem(g,md+0xF100,96)==flags and mem(g,md+0xF410,40)==inventory and r32(g,0xF438)==money
results.append({'case':'actual_event_RUN','returned_to_debug_menu':True,'story_inventory_money_unchanged':True})
source_rom=(R/'references/roms/ps4built.bin').read_bytes()
be=lambda a,n:int.from_bytes(source_rom[a:a+n],'big')
for tag,debug,full,drop in [('NORMAL_DROP',0,0,1),('NORMAL_FULL',0,1,1),('NORMAL_NONE',0,0,0),('DEBUG_RESULTS',1,0,1)]:
 g=GBA(rom,entry=0x08800000);g.load(str(state));pp,ee=0x0200dd60,0x0200dea0
 # A native actor at one source EXP threshold below level two; preserve its
 # actual source record except controlled level/EXP/learned-slot fixtures.
 w16(g,0xF508,1);w32(g,0xF50A,be(0x2856B0,4)-1)
 put(g,md+0xF552,bytes(16));put(g,md+0xF562,bytes(24))
 call(g,'battle_load_char',(0,pp));g.wr16(pp,999);g.wr8(pp+27,0)
 call(g,'battle_load_enemy',(9,ee));g.wr16(ee,0)
 g.wr32(s['battle_enemy_count'],1);g.wr8(s['battle_enemy_reward_mask'],1)
 g.wr8(s['battle_event_index'],1);g.wr8(s['battle_debug_battle_active'],debug)
 put(g,md+0xF410,bytes([0x91])*40 if full else bytes(40));w32(g,0xF438,100)
 g.wr8(md+0x41F2,127 if drop else 0);g.wr8(md+0x41F3,0x4E)
 call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,1,ee,0))
 call(g,'battle_compose_igg',(pp,1,ee,0,0));call(g,'battle_present_page0')
 before_actor=mem(g,pp,62);before_enemy=mem(g,ee,62);xp=r32(g,0xF50A)
 begin(g,'battle_show_results',(ee,pp,1))
 L.gba_set_break(ctypes.c_uint32(0x08800000));g.run(100)
 g.shot(str(O/(prefix+'_RESULTS_'+tag+'.png')),scale=2)
 exp=be(0x2816BC+9*48+44,2);meseta=be(0x2816BC+9*48+46,2)
 assert r32(g,0xF50A)==xp+(0 if debug else exp)
 assert r32(g,0xF438)==100+(0 if debug else meseta)
 assert r16(g,0xF508)==(1 if debug else 2)
 if not debug and drop:
  assert mem(g,md+0xF410,40)==(bytes([0x91])*40 if full else bytes([0x4E])+bytes(39))
 else:assert mem(g,md+0xF410,40)==(bytes([0x91])*40 if full else bytes(40))
 assert mem(g,pp,62)==before_actor and mem(g,ee,62)==before_enemy
 snapshot=mem(g,md+0xF410,40)+mem(g,md+0xF438,4)+mem(g,md+0xF500,128)
 assert press_to_break(g,0x08800000),'Results input did not return'
 assert mem(g,md+0xF410,40)+mem(g,md+0xF438,4)+mem(g,md+0xF500,128)==snapshot,'Results acknowledgement repeated rewards'
 results.append({'case':tag,'exp_meseta_level_correct':True,'item_outcome_correct':True,'acknowledgement_does_not_repeat_reward':True,'actors_unchanged':True})
 print('PASS native results',tag,flush=True)
(O/'R46_RESULTS_EMULATOR_EVIDENCE.json').write_text(json.dumps({'core':'Custom HLE / non-cycle-accurate / no audio output','fixtures':'Debug boot, actual event RUN route, native result calls with source actor/enemy, high source drop rate 127 and zero rate for no-drop branch; native RNG runs unchanged','cases':results,'normal_save_or_hardware_verified':False},indent=2)+'\n')
