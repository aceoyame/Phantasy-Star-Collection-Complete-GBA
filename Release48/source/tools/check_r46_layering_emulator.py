#!/usr/bin/env python3
"""Native Igglanova action priorities and PD3 workspace bounds.

Uses one return breakpoint per action; samples OAM without mid-frame hooks.
Custom HLE evidence, not original VDP parity or hardware timing proof.
"""
from pathlib import Path
import argparse,ctypes,json,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
ap=argparse.ArgumentParser();ap.add_argument('--prefix',default='CP12');args=ap.parse_args();prefix='R46_'+args.prefix
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
def begin(g,name,args=()):
 for i,v in enumerate(args[:4]):L.gba_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(i,v)
def call(g,name,args=()):
 begin(g,name,args);assert run_until_break(g,0x08800000,100),name
def load():
 g=GBA(rom,entry=0x08800000);g.load(str(O/(prefix+'_DF3_MENU.bin')));return g
def active(g,i):
 a=g.rd16(0x07000000+i*8)
 return a!=160 and (bool(a&0x0100) or not(a&0x0200))
def priority(g,i):return (g.rd16(0x07000004+i*8)>>10)&3
pp,ee,cp,tp=0x0200dd60,0x0200dea0,0x0200dfb0,0x0200dfe0
results=[]
for tag in ['SWORD','FOI']:
 g=load()
 for i,actor in enumerate([0,5,3,6,9]):call(g,'battle_load_char',(actor,pp+i*62))
 # Source Chaz right-hand short sword; keep the source actor/tech data.
 g.wr8(0x0200f54c,1);g.wr16(pp+4,999)
 for i,enemy in enumerate([9,12,9]):
  call(g,'battle_load_enemy',(enemy,ee+i*62));g.wr16(ee+i*62,999)
  g.wr8(s['battle_enemy_placement']+i,[16,23,30][i])
 g.wr32(s['battle_enemy_count'],3);g.wr8(s['battle_event_index'],0)
 call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,5,ee,1))
 call(g,'battle_compose_igg',(pp,5,ee,1,0));call(g,'battle_present_page0');call(g,'battle_obj_sync',(ee,3))
 assert priority(g,1)==2,'Idle Igg must be behind priority 0/1 effects'
 for i,v in enumerate([1 if tag=='SWORD' else 2,0 if tag=='SWORD' else 1,1,255]):g.wr8(cp+i,v)
 g.wr32(tp,1)
 if tag=='SWORD':begin(g,'battle_party_source_attack',(pp,5,ee,1,0,cp));slots=range(64,112)
 else:begin(g,'battle_party_foi_source_technique',(pp,5,ee,0,cp,tp));slots=range(40,44)
 L.gba_set_break(ctypes.c_uint32(0x08800000));seen={};frames=0;shots=[];shot_slots=set()
 for frame in range(2000):
  g.run(1);frames+=1
  if L.gba_break_hit():break
  for slot in slots:
   if active(g,slot):
    assert priority(g,slot)<2,(tag,slot,priority(g,slot))
    assert not active(g,1) or priority(g,1)==2
    if slot in seen and frame>seen[slot] and slot not in shot_slots:
     path=prefix+'_IGG_'+tag+'_OBJ'+str(slot)+'.png'
     if len(shots)<4:g.shot(str(O/path),scale=2);shots.append(path);shot_slots.add(slot)
    seen.setdefault(slot,frame)
 else:raise AssertionError('Action did not return: '+tag)
 L.gba_set_break(ctypes.c_uint32(0xffffffff))
 assert seen,(tag,'no source effect objects sampled')
 assert g.reg(0)==1,(tag,'source action did not execute')
 results.append({'case':tag,'idle_priority':2,'effect_slots_seen':sorted(seen),'all_effect_priorities_in_front':True,'display_frames':frames,'screenshots':shots})
 print('PASS native Igg',tag,sorted(seen),frames,flush=True)
# PD3 assembled mapping extends beyond the old 8192-byte body array.
g=load();call(g,'battle_load_enemy',(0x87,ee));g.wr32(s['battle_enemy_count'],1)
call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,1,ee,0));call(g,'r45_pd_visual_reset',(ee,))
base=0x0203b000;footer=base+16512;end=base+16568
before=bytes(g.rd8(footer+i) for i in range(56))
for i in range(64):g.wr8(end+i,0xA5)
call(g,'r45_pd_body_snapshot',(0x87,))
assert g.rd16(footer+2)==8384,'Unexpected source PD3 assembly size'
after=bytes(g.rd8(footer+i) for i in range(56))
assert after[:2]==before[:2] and after[4:]==before[4:],'PD3 changed neighboring state'
assert bytes(g.rd8(end+i) for i in range(64))==bytes([0xA5])*64,'PD3 overran its workspace'
results.append({'case':'PD3_BODY_BOUNDS','assembled_bytes':8384,'capacity':9216,'neighbor_state_and_guard_unchanged':True})
print('PASS native PD3 body bounds',flush=True)
(O/(prefix+'_LAYERING_EVIDENCE.json')).write_text(json.dumps({'core':'Bundled custom HLE; non-cycle-accurate, no audio output','cases':results,'full_attack_palette_position_parity':False},indent=2)+'\n')
