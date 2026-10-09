#!/usr/bin/env python3
"""Measure native PD1 opening on the bundled core, without per-tick breaks.

Instruction/frame counts are diagnostic only, not GBA cycle or audio proof.
Requires a matching-ROM menu state from check_r46_results_emulator.py.
"""
from pathlib import Path
import argparse,ctypes,json,re,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
ap=argparse.ArgumentParser();ap.add_argument('--prefix',default='CP10');args=ap.parse_args();prefix='R46_'+args.prefix
g=GBA(str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba'),entry=0x08800000)
g.load(str(O/(prefix+'_DF3_MENU.bin')))
def call(name,args=(),limit=100):
 for reg,val in enumerate(args[:4]):L.gba_set_reg(reg,val)
 for i,val in enumerate(args[4:]):g.wr32(0x03007d80+i*4,val)
 for reg,val in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(reg,val)
 start=g.frame();ins=L.gba_insns();assert run_until_break(g,0x08800000,limit),name
 return {'display_frames':g.frame()-start,'instructions':int(L.gba_insns()-ins)}
pp,ee=0x02033600,0x02033800
call('battle_load_char',(0,pp));call('battle_load_enemy',(0x85,ee))
g.wr32(s['battle_enemy_count'],1);g.wr8(s['battle_event_index'],1);g.wr8(s['battle_enemy_placement'],0x14)
call('battle_anim_reset');call('battle_ui_bind',(pp,1,ee,0));call('r45_pd_visual_reset',(ee,))
call('battle_compose_igg',(pp,1,ee,0,0));call('battle_present_page0')
L.gba_prof(2)
profile=call('r45_pd_opening',(pp,1,ee),10000)
L.gba_prof(0)
L.gba_prof_hist.restype=ctypes.POINTER(ctypes.c_uint32)
L.gba_prof_iw.restype=ctypes.POINTER(ctypes.c_uint32)
hist=L.gba_prof_hist();iw=L.gba_prof_iw();regions=[]
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5 and re.fullmatch(r'[a-zA-Z_][a-zA-Z_0-9]*',p[4]) and p[2]!='0':
  try:a,n=int(p[0],16),int(p[2],16)
  except ValueError:continue
  if 0x08800000<=a<0x0A000000:v=sum(hist[i] for i in range((a-0x08800000)//16,(a+n-0x08800000+15)//16))
  elif 0x03000000<=a<0x03008000:v=sum(iw[i] for i in range((a-0x03000000)//16,(a+n-0x03000000+15)//16))
  else:continue
  if v:regions.append({'section':p[4],'estimated_core_cycles':v})
profile['hot_sections']=sorted(regions,key=lambda x:-x['estimated_core_cycles'])[:20]
g.shot(str(O/(prefix+'_PD1_PROFILE.png')),scale=2)
profile.update({'source_ticks':192,'core':'Bundled custom HLE, non-cycle-accurate, no audio output','per_tick_breakpoints':False,'real_device_frame_time_certified':False,'scope':'Native opening with loaded Chaz/PD1 actor records; no dropped updates or ROM hooks. Diagnostic comparison; hardware timing remains open.'})
(O/(prefix+'_PD1_PROFILE.json')).write_text(json.dumps(profile,indent=2)+'\n');print(json.dumps(profile),flush=True)
