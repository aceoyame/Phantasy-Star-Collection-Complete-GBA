#!/usr/bin/env python3
"""Native PD polling latency and real menus; old-ROM controlled A/B."""
from pathlib import Path
import argparse,sys
p=argparse.ArgumentParser(add_help=False);p.add_argument('--baseline-dir',type=Path,required=True);opt,rest=p.parse_known_args();sys.argv=[sys.argv[0]]+rest
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0],str(h),'exec'),globals())
current_symbols=symbols.copy();current_rom=rom;current_state=state
rows=[]
def parse_map(path):
 d={}
 for line in path.read_text().splitlines():
  q=line.split()
  if len(q)>=5:
   try:d[q[-1]]=int(q[0],16)
   except ValueError:pass
 return d
def diag():return [g.rd32(symbols['ps4_audio_diag']+i*4) for i in range(4)]
def setup(id_):
 g.load(st);call('ps4_debug_level60_party');g.wr8(symbols['ps4_menu_overlay_active'],0)
 for i,cid in enumerate(g.mem(md+0xf40a,5)):call('battle_load_char',(cid,pp+i*62))
 call('battle_load_enemy',(id_,ee));g.wr32(symbols['battle_enemy_count'],1);g.wr8(symbols['battle_event_index'],1);g.wr8(symbols['battle_enemy_placement'],20)
 call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('r45_pd_visual_reset',(ee,));g.put(0x0203b000+16532,bytes([0,1,2,3,4,5,0,0,0,0,0,0]));g.put(0x0203b000+16556,bytes([1]*9+[0]*3));call('r45_pd_body_snapshot',(id_,));call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0')
for which in ('release09','current'):
 if which=='release09':symbols=parse_map(opt.baseline_dir/'release09.map');g=GBA(opt.baseline_dir/'release09.gba');st=opt.baseline_dir/'release09-debug.bin'
 else:symbols=current_symbols;g=GBA(current_rom);st=current_state
 for id_ in (0x85,0x86,0x87):
  setup(id_);d=diag();f=g.frame();cy=g.cycles();scene=g.mem(0x06000000,38400);body=g.mem(symbols['battle_enemy_anim_frame'],18)+g.mem(0x0203b080,768)+g.mem(0x0203b000+16531,1)
  for i in range(120):call('battle_ui_idle_vblank')
  row={'ROM':which,'form':id_,'polls':120,'display_frames':g.frame()-f,'CPU_cycles':g.cycles()-cy,'audio_delta':[v-u for u,v in zip(d,diag())]};rows.append(row)
  if which=='current':
   assert row['display_frames']==120 and row['audio_delta'][2]==0,row
   assert scene==g.mem(0x06000000,38400)
   assert body==g.mem(symbols['battle_enemy_anim_frame'],18)+g.mem(0x0203b080,768)+g.mem(0x0203b000+16531,1)
   for phase in range(9):
    g.wr8(tp,0);begin('battle_ui_wait_edge',(tp,));L.mg_break(0x08800000)
    if phase:g.run(phase)
    assert not L.mg_hit()
    g.run(1,'DOWN')
    if not L.mg_hit():g.run(3)
    assert L.mg_hit() and g.reg(0)&2,(id_,phase,'one-frame phase pulse');L.mg_break(0xffffffff)
  print('PASS polling',row,flush=True)
  if which=='release09':continue
  # Each actual menu consumes one-display-frame directional/confirm/cancel
  # presses at its ready input loop, including list paging and targeting.
  def ready(name,args):
   begin(name,args);assert run_until_break(g,symbols['battle_ui_wait_edge'],100),name
  def pulse(key,last=False):
   ret=g.reg(14);L.mg_break(ret);g.run(1);assert not L.mg_hit();f=g.frame();g.run(1,key)
   if not L.mg_hit():g.run(3)
   assert L.mg_hit(),('one-frame press missed',id_,key)
   frames=g.frame()-f;edge=g.reg(0);L.mg_break(0xffffffff)
   assert frames<=2,(id_,key,frames)
   assert run_until_break(g,0x08800000 if last else symbols['battle_ui_wait_edge'],100),key+' menu continuation'
   return frames,edge
  ready('battle_choose_main_option',(1,));pulse('DOWN');pulse('A',True);assert g.reg(0)==1
  ready('battle_command_for_actor',(pp,cp));pulse('LEFT');pulse('A',True);assert g.reg(0)==1 and g.rd8(cp)==5
  ready('battle_choose_ability',(pp,0,tp));pulse('DOWN');pulse('RIGHT');pulse('LEFT');pulse('B',True);assert g.reg(0)==0
  ready('battle_choose_ability',(pp,1,tp));pulse('DOWN');pulse('A',True);assert g.reg(0)==1 and g.rd8(tp)!=0
  ready('battle_command_for_actor',(pp,cp));pulse('RIGHT');pulse('RIGHT');pulse('RIGHT');pulse('A');pulse('RIGHT');pulse('LEFT');pulse('B');pulse('B',True);assert g.reg(0)==0
  ready('battle_choose_ally',(pp,5,0,4));pulse('RIGHT');pulse('A',True);assert g.reg(0)==1
  ready('battle_choose_target',(pp,5,ee,0,0));pulse('B',True);assert g.reg(0)==0xffffffff
  ready('battle_choose_target',(pp,5,ee,0,0));pulse('A',True);assert g.reg(0)==0
  # Pending held confirm is released through the same one-VBlank path.
  begin('battle_choose_main_option',(1,));L.mg_break(symbols['battle_ui_wait_edge']);g.run(3,'A');assert not L.mg_hit();g.run(20);assert L.mg_hit();L.mg_break(0xffffffff)
  print('PASS all actual PD menus / one-frame presses / release',hex(id_),flush=True)
# Ordinary generic enemies continue their original animation/background path.
setup(9);f=g.frame();call('battle_ui_idle_vblank');assert g.frame()>f
(O/(prefix+'_MGBA_PD_MENUS.json')).write_text(json.dumps({'rom':rom_id,'polling_AB':rows,'all_three_forms_actual_main_command_TECH_SKILL_ITEM_ally_enemy_menus':True,'one_frame_presses_and_held_release':True,'one_frame_pulse_idle_phases_per_form':9,'total_phase_pulses':27,'input_loop_press_return_bound_display_frames':2,'PD_idle_framebuffer_art_scroll_animation_retained':True,'ordinary_enemy_idle_path_retained':True,'workaround':'Pause expensive PD idle simulation only during menu wait; palette and audio continue, actions retain source updates','limits':'controlled mGBA fixtures; no physical hardware or uninterrupted normal-save certification'},indent=2)+'\n')
