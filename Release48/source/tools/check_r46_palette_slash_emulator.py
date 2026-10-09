#!/usr/bin/env python3
"""Native melee palette ownership and source slash-body/target anchors."""
from pathlib import Path
import argparse,ctypes,json,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
ap=argparse.ArgumentParser();ap.add_argument('--prefix',default='CP13');a=ap.parse_args();prefix='R46_'+a.prefix
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
raw=(R/'references/roms/ps4built.bin').read_bytes()
def md(c):
 t=[0,4,9,13,18,22,27,31];return t[c>>1&7]|t[c>>5&7]<<5|t[c>>9&7]<<10
body=[0]+[md(int.from_bytes(raw[0x76a4+i*2:0x76a6+i*2],'big')) for i in range(15)]
def mem(g,p,n):return bytes(g.rd8(p+i) for i in range(n))
def put(g,p,b):
 for i,v in enumerate(b):g.wr8(p+i,v)
def begin(g,name,args=()):
 for i,v in enumerate(args[:4]):L.gba_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(i,v)
def call(g,name,args=()):begin(g,name,args);assert run_until_break(g,0x08800000,100),name
pp,ee,cp,tp=0x0200dd60,0x0200dea0,0x0200dfb0,0x0200dfe0
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');cases=[]
def setup(ids,enemy,placement):
 g=GBA(rom,entry=0x08800000);g.load(str(O/(prefix+'_DF3_MENU.bin')))
 for i,id in enumerate(ids):call(g,'battle_load_char',(id,pp+i*62))
 for i,p in enumerate(placement):
  call(g,'battle_load_enemy',(enemy,ee+i*62));g.wr16(ee+i*62,999);g.wr16(ee+i*62+2,999);g.wr8(s['battle_enemy_placement']+i,p)
 g.wr32(s['battle_enemy_count'],len(placement));g.wr8(s['battle_event_index'],1)
 call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,len(ids),ee,0));call(g,'battle_compose_igg',(pp,len(ids),ee,0,0));call(g,'battle_present_page0');return g
for tag,actor,item in [('CHAZ_SWORD',0,8),('RUNE_CANE',3,16),('HIGH_SELECTOR',0,3)]:
 g=setup([actor],0x80,[20]);put(g,0x0200f500+actor*128+0x4c,bytes([item,0]));put(g,cp,bytes([1,0,0,255]));before=mem(g,pp,62)+mem(g,ee,62)
 begin(g,'battle_party_source_attack',(pp,1,ee,0,0,cp));L.gba_set_break(ctypes.c_uint32(0x08800000));seen=set();frames=0;samples=0
 for frame in range(2000):
  g.run(1);frames+=1
  if L.gba_break_hit():break
  live=[]
  for oi in range(64,112):
   if g.rd16(0x07000000+oi*8)&0x0100:
    bank=g.rd16(0x07000004+oi*8)>>12;assert bank in (14,15);seen.add(bank);live.append(bank)
  if 14 in live:
   assert [g.rd16(0x05000200+224*2+i*2) for i in range(16)]==body
   samples+=1
   if samples==3:g.shot(str(O/(prefix+'_'+tag+'.png')),scale=2)
 assert L.gba_break_hit() and samples and seen=={14,15},(tag,samples,seen)
 assert mem(g,pp,62)+mem(g,ee,62)==before
 cases.append({'case':tag,'body_palette_matches_source_line3':True,'banks_seen':sorted(seen),'visual_preserves_actor_hp':True,'display_frames':frames});print('PASS native palettes',tag,flush=True)
for skill in [1,2]:
 g=setup([0],0x89,[12,32]);p=bytearray(mem(g,pp,62));p[:4]=(999).to_bytes(2,'little')*2;p[25:29]=bytes([255,255,0,0]);put(g,pp,p)
 for i in range(2):g.wr8(ee+i*62+25,0);g.wr8(ee+i*62+27,0)
 d=0x0200f500;put(g,d+0x4c,bytes([8,0]));put(g,d+0x62,bytes([skill])+bytes(7));put(g,d+0x6a,bytes([3])+bytes(15));put(g,cp,bytes([3,skill,1,255])+bytes(16));g.wr32(tp,1)
 before=mem(g,ee,124);begin(g,'battle_resolve_mixed_round',(pp,1,ee,cp,0xffffffff,tp))
 assert run_until_break(g,s['battle_ability_tracks_draw'],100),'Missing slash tracks'
 assert g.rd32(s['battle_ability_track_count'])==2
 tracks=[]
 for i in range(2):
  q=0x02033d80+i*32;anim=g.rd32(q);xs=g.rd16(q+10);ys=g.rd16(q+20);nt=g.rd8(q+8);table=g.rd32(anim+8)
  tracks.append({'table':hex(table),'start':g.rd16(q+4),'x':xs,'y':ys,'nt':nt})
 assert tracks[0]=={'table':hex(0x25e68a if skill==1 else 0x25e5fe),'start':16,'x':192,'y':57,'nt':1},tracks
 assert tracks[1]['table']==hex(0x25f792 if skill==1 else 0x25f7ba) and tracks[1]['start']==0 and tracks[1]['x']==205 and tracks[1]['y']==97 and tracks[1]['nt']==129,tracks
 assert not g.rd8(s['battle_party_visible_mask'])&1
 assert mem(g,ee,124)==before and g.rd8(d+0x6a)==3
 L.gba_set_break(ctypes.c_uint32(s['battle_execute_cmd']));seen_banks=set();shots=[]
 for frame in range(2000):
  g.run(1)
  if L.gba_break_hit():break
  banks={g.rd16(0x07000004+oi*8)>>12 for oi in range(64,112) if g.rd16(0x07000000+oi*8)&0x100}
  assert banks.issubset({14,15});seen_banks.update(banks)
  if frame%12==0 and banks and len(shots)<6:
   path=prefix+('_CROSSCUT' if skill==1 else '_RAYBLADE')+'_FRAME'+str(frame)+'.png';g.shot(str(O/path),scale=2);shots.append(path)
 assert L.gba_break_hit() and seen_banks=={14,15},seen_banks
 L.gba_set_break(ctypes.c_uint32(0xffffffff))
 assert mem(g,ee,124)==before and g.rd8(d+0x6a)==3
 g.shot(str(O/(prefix+('_CROSSCUT' if skill==1 else '_RAYBLADE')+'_LAST_VISUAL.png')),scale=2)
 lr=g.reg(14);assert run_until_break(g,lr&~1,50)
 assert g.rd8(d+0x6a)==2 and g.rd16(ee)==999 and g.rd16(ee+62)<999
 assert g.rd8(s['battle_party_visible_mask'])&1
 cases.append({'case':'CROSSCUT' if skill==1 else 'RAYBLADE','tracks':tracks,'unchanged_hp_and_uses_until_gameplay_commit':True,'one_skill_use_only_target_damaged':True,'party_visibility_restored':True,'body_and_trail_banks_seen':sorted(seen_banks),'screenshots':shots});print('PASS native slash',skill,tracks,flush=True)
(O/(prefix+'_PALETTE_SLASH_EVIDENCE.json')).write_text(json.dumps({'core':'Bundled HLE, non-cycle-accurate/no audio output','cases':cases,'scope':'Native visual paths and actual mixed-round skill commit. Equipment/stats/targets/learned slots are fixtures; original full VDP timing remains open.'},indent=2)+'\n')
