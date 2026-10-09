#!/usr/bin/env python3
"""Illusion source blink/clone/motion fixtures; unchanged gameplay oracle."""
from pathlib import Path
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0],str(h),'exec'),globals())
import re
assert 'if(c->id==0x1Cu)return battle_party_source_illusion_skill(' in (S/'gba/title_runtime_v06.c').read_text()
catalog=(S/'gba/source_ability_effects.c').read_text();bank=(S/'gba/source_ability_effects.bin').read_bytes()
def rows(t,n):return [[int(v.rstrip('u'),0) for v in line.strip('{};,').split(',')] for line in catalog.split(f'const {t} {n}[]={{')[1].split('};')[0].splitlines() if line.startswith('{')]
parts=rows('Ps4SourceAbilityPart','ps4_source_ability_parts');anims=rows('Ps4SourceAbilityAnim','ps4_source_ability_anims')
a=next(v for v in anims if v[-1]==0x25fcca);aparts=parts[a[0]:a[0]+a[1]]
def trunc(n,d):return abs(n)//d*(-1 if n<0 else 1)
def active():return [(g.rd16(0x07000000+i*8),g.rd16(0x07000002+i*8),g.rd16(0x07000004+i*8)) for i in range(64,112) if g.rd16(0x07000000+i*8)&255!=160]
for mode in ('hit','mixed','miss','immune','sealed','depleted','unknown','dead'):
 ability_fixture(True,0x1c,[(3,100,0),(5,100,0),(6,100,0),(7,100,0),(9,100,0)],uses=0 if mode=='depleted' else 1)
 g.wr8(pp+25,37)
 if mode=='sealed':g.wr8(pp+27,16)
 if mode=='unknown':g.wr8(md+0xf500+3*128+0x62,0)
 if mode=='dead':g.wr16(pp,0)
 for i in range(4):
  call('battle_load_enemy',(9,ee+i*62));g.wr16(ee+i*62,1000);g.wr8(ee+i*62+25,200);g.wr8(ee+i*62+32,100)
  g.wr8(ee+i*62+24,255 if mode=='miss' or mode=='mixed' and i==1 else 0)
  g.wr8(ee+i*62+27,4 if mode=='mixed' and i==2 else 0)
  if mode=='mixed' and i==3:g.wr16(ee+i*62,0)
  for j in range(14):g.wr8(ee+i*62+34+j,0 if mode=='immune' else 4)
 g.wr32(symbols['battle_enemy_count'],4);g.put(symbols['battle_enemy_placement'],bytes([8,16,24,32]));call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0')
 for i in range(64,112):g.put(0x07000000+i*8,bytes([160,0,0,0,0,0,0,0]))
 g.put(cp,bytes([3,0x1c,0,255]));g.wr32(tp,0);w32(0xef0c,0x12345678)
 fixture_state=O/(prefix+'_ILLUSION_FIXTURE.bin');g.save(fixture_state)
 g.wr8(symbols['battle_suppress_skill_source_audio'],1);call('battle_execute_cmd',(pp,5,ee,0,cp,tp))
 oracle=g.mem(pp,310)+g.mem(ee,248)+g.mem(md+0xf100,0xa00)+g.mem(md+0xef0c,4)
 g.load(fixture_state);allowed=mode not in ('depleted','unknown','dead')
 if allowed:
  begin('battle_party_source_illusion_skill',(pp,5,ee,0,cp,tp))
  for tick in range(97):
   assert run_until_break(g,symbols['battle_action_idle_after_wait'],30),(mode,tick)
   positions=[]
   if tick<16:
    if tick in (0,5,9,12,13):positions=[120]
   elif tick<96:
    for child,speed in enumerate((0,-4,-2,-1,1,2,4)):
     steps=tick-16+(child!=0)
     if not ((child-steps)&2):positions.append(120+trunc((((576+speed*steps)//2)-288)*3,4))
   expected=[]
   for off,n,geom,_,ox,oy,_ in aparts:
    shape,size=geom&3,(geom>>2)&3;hw=[[8,16,32,64],[16,32,32,64],[8,8,16,32]][shape][size]//2;hv=[[8,16,32,64],[8,8,16,32],[16,32,32,64]][shape][size]//2
    for x in positions:expected.append(((((0x108-0x80)*5//7+trunc((oy+hv)*5,7)-hv)&255)|0x100|(shape<<14),((x+trunc((ox+hw)*3,4)-hw)&511)|(31<<9)|(size<<14),off,n))
   objs=active();assert len(objs)==len(expected),(mode,tick,len(objs),len(expected))
   for (a0,a1,a2),(e0,e1,off,n) in zip(objs,expected):
    assert (a0,a1)==(e0,e1),(mode,tick,'geometry');assert a2>>12==15 and (a2>>10)&3==1
    assert g.mem(0x06010000+(a2&1023)*32,n)==bank[off:off+n]
   assert g.mem(pp,310)+g.mem(ee,248)+g.mem(md+0xf100,0xa00)+g.mem(md+0xef0c,4)==oracle,(mode,tick,'one-use semantics')
   if tick==0:assert g.rd8(md+0x500a)==0xca
   ret=g.reg(14);assert run_until_break(g,ret,30)
  assert run_until_break(g,0x08800000,30) and g.reg(0)==1
  assert not active() and g.rd8(symbols['battle_party_pose_actor'])==255
 else:
  assert call('battle_party_source_illusion_skill',(pp,5,ee,0,cp,tp))==0
  assert g.mem(pp,310)+g.mem(ee,248)+g.mem(md+0xf100,0xa00)+g.mem(md+0xef0c,4)==oracle
 results.append({'case':mode,'source_updates':97 if allowed else 0,'source_art_geometry_visibility_signed_motion':allowed,'existing_gameplay_resources_RNG_unchanged':True});print('PASS ILLUSION',mode,flush=True)
fixture_state.unlink()
(O/(prefix+'_MGBA_ILLUSION.json')).write_text(json.dumps({'rom':rom_id,'cases':results,'limits':'Controlled native fixtures and disassembly-derived timeline; no full 68000 clock, ordinary battle or hardware certification'},indent=2)+'\n')
