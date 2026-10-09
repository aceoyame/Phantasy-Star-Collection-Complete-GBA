#!/usr/bin/env python3
"""Fresh-ROM actual event-4 command loop and Alys field dispatch, custom HLE.
Not original VDP parity, audio output, cycle timing, or normal-save playthrough.
"""
from pathlib import Path
import argparse,ctypes,json,re,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--core',choices=['hle','mgba'],default='hle');ap.add_argument('--label',default='CP16');a=ap.parse_args()
if a.core=='mgba':
 sys.path.insert(0,str(R/'tools_host/r46_mgba'));import mgba
 sys.modules['gba']=mgba
 mgba.L.gba_set_reg=mgba.L.mg_set_reg;mgba.L.gba_set_break=mgba.L.mg_break;mgba.L.gba_break_hit=mgba.L.mg_hit
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
from boot_debug import boot_to_debug
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');md=0x02000000
prefix='R46_'+a.label+'_ZIO_'+a.core.upper()
print('Booting fresh ROM',flush=True)
g=boot_to_debug(rom,str(O/(prefix+'_DEBUG.bin')))
g.save(str(O/(prefix+'_FRESH.bin')));print('Fresh boot reached debug selector',flush=True)
def mem(a,n):return bytes(g.rd8(a+i) for i in range(n))
def put(a,b):
 for i,v in enumerate(b):g.wr8(a+i,v)
def w16(a,v):put(md+a,v.to_bytes(2,'big'))
def r16(a):return int.from_bytes(mem(md+a,2),'big')
def flag(f,setto):
 a=md+0xF100+(f>>3);bit=1<<(7-(f&7));g.wr8(a,(g.rd8(a)|bit) if setto else g.rd8(a)&~bit)
def begin(name,args=()):
 L.gba_set_break(ctypes.c_uint32(0xffffffff))
 if a.core=='mgba':L.mg_cpsr(0x1f)
 for i,v in enumerate(args[:4]):L.gba_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(i,v)
def call(name,args=()):
 begin(name,args);assert run_until_break(g,0x08800000,150),name
# Controlled source party resources; commands and initiative run unchanged.
ids=[0,1,2,4,5];put(md+0xf40a,bytes(ids));w16(0xf43c,0)
for i,cid in enumerate(ids):
 d=0xf500+cid*128;w16(d+0xe,r16(d+0x10));w16(d+0x12,r16(d+0x14));g.wr8(md+d+0x16,0)
 put(md+0xf444+i*4,bytes([cid,5,0,0]))
g.wr8(s['battle_debug_battle_active'],0);g.wr8(md+0xecec,0xa9);flag(0x42,1);flag(0x47,0)
resources=lambda:[{'hp':r16(0xf500+i*128+0xe),'tp':r16(0xf500+i*128+0x12),'status':g.rd8(md+0xf500+i*128+0x16)} for i in ids]
before=resources();persistent=mem(md+0xf100,96)+mem(md+0xf410,44);xp=[mem(md+0xf500+i*128+0xa,4) for i in ids]
begin('battle_run_event',(4,));assert run_until_break(g,s['battle_choose_main_option'],300)
assert g.rd32(s['battle_enemy_count'])==1 and g.rd8(g.rd32(s['battle_ui_enemies'])+20)==0x98
assert g.rd8(md+0xee98)==0
rounds=[]
for stage in range(5):
 if a.core=='mgba':assert run_until_break(g,s['battle_ui_wait_edge'],1000),'Main menu never reached input'
 L.gba_set_break(ctypes.c_uint32(s['battle_choose_macro']))
 g.run(10);g.press('DOWN',30 if a.core=='mgba' else 3,30);g.press('A',30 if a.core=='mgba' else 3,20)
 assert L.gba_break_hit(),'MACR not selected'
 if a.core=='mgba':assert run_until_break(g,s['battle_ui_wait_edge'],1000),'Macro menu never reached input'
 target=0x08800000 if stage==4 else s['battle_choose_main_option']
 L.gba_set_break(ctypes.c_uint32(target));g.run(10);g.press('A',30 if a.core=='mgba' else 3,20)
 for ticks in range(6000 if a.core=='mgba' else 1200):
  if L.gba_break_hit():break
  g.run(1)
 else:raise AssertionError('Round failed to finish '+str(stage))
 assert g.rd8(md+0xee98)==stage+1
 assert resources()==before,'Zio story controller changed HP/TP/status'
 assert mem(md+0xf100,96)+mem(md+0xf410,44)==persistent,'Battle changed flags, inventory, or Meseta'
 assert [mem(md+0xf500+i*128+0xa,4) for i in ids]==xp,'Battle granted EXP'
 if stage<4:
  g.run(1);g.shot(str(O/(prefix+'_STAGE_'+str(stage)+'.png')),scale=2)
 rounds.append({'stage':stage,'phase_after':stage+1,'native_loop_returned':True,'resources_rewards_unchanged':True})
 print('PASS actual event-4 round',stage,flush=True)
assert g.reg(0)==2 and r16(0xef00)==8 and r16(0x4144)==2
assert not g.rd8(md+0xee68) and g.rd16(0x04000000)==0x44
assert not g.rd32(s['battle_generic_pending_mask']) and not g.rd8(s['battle_generic_stage_ready']) and not g.rd8(s['battle_palette_hw_dirty']), 'Battle staging leaked into field return'
# Both guard branches execute; the positive branch enters the EXISTING Alys
# scene through field_run_map_event_condition -> source event $8009.
coords=0x0200dfb0;g.wr32(coords,0);g.wr32(coords+4,0)
for zio,demi in [(0,0),(0,1),(1,1)]:
 flag(0x42,zio);flag(0x47,demi);call('field_run_map_event_condition',(0x1d,coords,coords+4));assert g.reg(0)==0
flag(0x42,1);flag(0x47,0);call('ps4_load_field_map',(0x8b,));assert g.reg(0)==0
plan=g.rd32(s['field_event_plan_ids']);count=g.rd8(s['field_event_plan_count']);assert 0x1d in mem(plan,count), 'Zio Fort F4 plan omitted wounded event'
begin('field_run_map_events',(coords,coords+4));assert run_until_break(g,s['r33_cutscene_alys_wounded'],100)
# Later forms go through the production generic executor; only the new story
# function's isolation is checked here without running unrelated cutscenes.
result={'core':'Upstream mGBA 0.10.5' if a.core=='mgba' else 'Custom HLE, non-cycle-accurate, no audio output','party_ids':ids,'fresh_production_ROM_boot':True,'actual_normal_event4_command_macro_loop':rounds,'return_code':2,'field_mode':8,'Alys_target_index':2,'no_EXP_Meseta_items_or_flags_granted':True,'wounded_event_guards':[{'Zio':0,'DemiJoined':0},{'Zio':0,'DemiJoined':1},{'Zio':1,'DemiJoined':1}],'positive_field_dispatch':'Actual Zio Fort F4 $8B map event plan -> RunEvent_AlysWounded -> $8009 -> existing r33_cutscene_alys_wounded','battle_staging_cleared_at_field_return':True,'full_cutscene_normal_save_hardware_verified':False}
(O/(prefix+'_EVIDENCE.json')).write_text(json.dumps(result,indent=2)+'\n');print('PASS actual field Alys dispatch; no generic battle rewards',flush=True)
