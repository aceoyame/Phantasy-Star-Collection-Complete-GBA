#!/usr/bin/env python3
"""Native PD1->2 and PD2->3 render cost, source callbacks and workspace guards.

Use a fresh matching-ROM state. One return breakpoint per complete controller;
memory samples do not interrupt production code. Not hardware/VDP timing proof.
"""
from pathlib import Path
import argparse,ctypes,json,re,struct,sys
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
ap=argparse.ArgumentParser();ap.add_argument('--prefix',default='CP12');args=ap.parse_args();prefix='R46_'+args.prefix
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
def mem(g,a,n):return bytes(g.rd8(a+i) for i in range(n))
def begin(g,name,args=()):
 for i,v in enumerate(args[:4]):L.gba_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(i,v)
def call(g,name,args=()):
 begin(g,name,args);assert run_until_break(g,0x08800000,100),name
pp,ee=0x02033600,0x02033800;footer=0x0203b000+16512;end=0x0203b000+16568
header=(R/'gba/precomp_assets.h').read_text()
def define(name):return int(re.search(r'#define '+name+r'\s+(\d+)',header)[1])
source_off=define('PS4_PRECOMP_BATTLE_ENEMY_NATIVE_SOURCE_OFF')
assets=(R/'gba/precomp_assets.bin').read_bytes();original=(R/'references/roms/ps4built.bin').read_bytes()
def source_body(g,enemy):
 # Parse original 68000 maps/selectors independently of the PD C renderer.
 m=define('PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_OFF')+enemy*define('PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_STRIDE')
 meta=struct.unpack_from('<8I',assets,m)
 src=assets[source_off+meta[0]:source_off+meta[0]+meta[1]]
 base=define('PS4_PRECOMP_BATTLE_ENEMY_NATIVE_BASE_OFF')+meta[2];body=bytearray(assets[base:base+meta[3]])
 u=lambda a,n:int.from_bytes(original[a:a+n],'big')
 table=0x10FCE;mapping=table+int.from_bytes(original[table+enemy*2:table+enemy*2+2],'big',signed=True)
 for op in range(u(mapping,2)+1):
  mp=mapping+2+op*6;desc=u(u(mp+2,4)&0x7fffffff,4);count=original[desc]&127
  frame=g.rd8(s['battle_enemy_anim_frame']+op)
  if not count or frame>=count:frame=0
  at=original[desc+(1+count+frame if original[desc]&128 else 2+frame)]*32
  dst=original[mp]*32;n=original[mp+1]*32
  assert at+n<=len(src) and dst+n<=9216
  if dst+n>len(body):body.extend(bytes(dst+n-len(body)))
  body[dst:dst+n]=src[at:at+n]
 return bytes(body)
def cpu_shot(g,path):
 # Canonical framebuffer memory avoids a return breakpoint's partial LCD frame.
 pixels=mem(g,0x0202a000,38400);colors=[]
 for i in range(256):
  c=g.rd16(0x02023400+i*2);colors.extend(((c&31)*255//31,((c>>5)&31)*255//31,((c>>10)&31)*255//31))
 im=Image.frombytes('P',(240,160),pixels);im.putpalette(colors);im.convert('RGB').resize((480,320),Image.Resampling.NEAREST).save(path)
results=[]
for form,ticks,skill in [(0x85,519,0x67),(0x86,595,0x68)]:
 g=GBA(rom,entry=0x08800000);g.load(str(O/(prefix+'_DF3_MENU.bin')))
 call(g,'battle_load_char',(0,pp));call(g,'battle_load_enemy',(form,ee))
 g.wr32(s['battle_enemy_count'],1);g.wr8(s['battle_event_index'],1);g.wr8(s['battle_enemy_placement'],0x14)
 call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,1,ee,0));call(g,'r45_pd_visual_reset',(ee,))
 sound=mem(g,0x0200500A,2);call(g,'r45_pd_body_snapshot',(form,));assert mem(g,0x0200500A,2)==sound,'PD snapshot changed sound commands'
 call(g,'battle_compose_igg',(pp,1,ee,0,0));call(g,'battle_present_page0')
 actor=mem(g,pp,62)
 for i in range(64):g.wr8(end+i,0xA5)
 cpu_shot(g,str(O/f'{prefix}_PD{form-0x84}_BEFORE_TRANSFORM.png'))
 start=g.frame();ins=L.gba_insns();begin(g,'r45_pd_transform',(pp,1,ee,0,skill));L.gba_set_break(ctypes.c_uint32(0x08800000))
 peak_body=peak_bg=0;frames=0
 for _ in range(15000):
  g.run(1);frames+=1
  assert mem(g,end,64)==bytes([0xA5])*64,'PD workspace guard changed'
  body,bg=g.rd16(footer+2),g.rd16(footer)
  assert body<=9216 and bg<=6400,(form,body,bg)
  assert g.rd8(footer+12)<3 and g.rd8(footer+14)<3,'PD background index corrupted'
  assert g.rd8(footer+13) in [0x85,0x86,0x87],'PD body ID corrupted'
  peak_body=max(peak_body,body);peak_bg=max(peak_bg,bg)
  if L.gba_break_hit():break
 else:raise AssertionError('PD transform did not return')
 L.gba_set_break(ctypes.c_uint32(0xffffffff))
 assert g.reg(0)==1 and g.rd8(ee+20)==form+1,'Source stat callback did not change form'
 assert mem(g,pp,62)==actor,'PD transform changed party resources'
 assert g.rd8(footer+10)==1 and g.rd8(footer+11)==0,'Visual controller did not finish'
 elapsed,insns=g.frame()-start,int(L.gba_insns()-ins)
 g.save(str(O/f'{prefix}_PD{form-0x84}_TRANSFORM_END.bin'))
 cpu_shot(g,str(O/f'{prefix}_PD{form-0x84}_AFTER_TRANSFORM.png'))
 meta=s['ps4_precomp_assets']+define('PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_OFF')+(form+1)*define('PS4_PRECOMP_BATTLE_ENEMY_NATIVE_META_STRIDE')
 src,n=g.rd32(meta),g.rd32(meta+4)
 # The decoded source is read from its immutable bank; the work mirror owns
 # live audio commands rather than a second copy of that bank.
 old_body=mem(g,0x0203b000+7296,9216)
 canonical_body=source_body(g,form+1);body_n=len(canonical_body)
 body_diff=sum(a!=b for a,b in zip(old_body[:body_n],canonical_body[:body_n]))
 assert body_diff==0,(form,body_diff)
 results.append({'from_form':form,'to_form':form+1,'source_ticks':ticks,'display_frames':elapsed,'instructions':insns,'peak_body_bytes':peak_body,'peak_background_bytes':peak_bg,'workspace_guard_unchanged':True,'background_indices_and_body_id_in_bounds':True,'stat_callback_changes_form':True,'party_resources_unchanged':True,'snapshot_preserves_sound_commands':True,'body_differing_bytes_vs_canonical_assembly':body_diff,'canonical_assembled_bytes':body_n,'images':'Canonical CPU framebuffer memory; not partial LCD return frame','endpoint_state_saved_before_canonical_snapshot':True})
 print('PASS native PD transform',json.dumps(results[-1]),flush=True)
(O/(prefix+'_PD_FORMS_PROFILE.json')).write_text(json.dumps({'core':'Bundled custom HLE; non-cycle-accurate, no audio output','per_tick_breakpoints':False,'cases':results,'real_device_frame_time_or_full_VDP_certified':False},indent=2)+'\n')
