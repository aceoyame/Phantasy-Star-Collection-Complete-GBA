#!/usr/bin/env python3
"""Cycle histogram for a short native PD controller window, upstream mGBA."""
from pathlib import Path
import argparse,ctypes,json,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP16');ap.add_argument('--frames',type=int,default=180);a=ap.parse_args()
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
s={};syms=[]
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:
   addr=int(p[0],16);s[p[-1]]=addr
   if len(p)==5 and int(p[2],16) and not p[4].startswith(('.', '/')):syms.append((addr,int(p[2],16),p[4]))
  except ValueError:pass
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');g.load(O/('R46_'+a.label+'_MGBA_DEBUG.bin'))
def begin(name,args=()):
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,v in enumerate(args[:4]):L.mg_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.mg_set_reg(i,v)
def call(name,args=()):
 begin(name,args);assert run_until_break(g,0x08800000,300),name
pp,ee=0x02033600,0x02033800;call('ps4_debug_level60_party');ids=[g.rd8(0x0200f40a+i) for i in range(5)]
for i,id in enumerate(ids):call('battle_load_char',(id,pp+i*62))
call('battle_load_enemy',(0x85,ee));g.wr32(s['battle_enemy_count'],1);g.wr8(s['battle_enemy_placement'],20);g.wr8(s['battle_event_index'],1)
call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('r45_pd_visual_reset',(ee,));call('r45_pd_body_snapshot',(0x85,));call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0')
begin('r45_pd_opening',(pp,5,ee));L.mg_break(0x08800000);L.mg_prof(1);g.run(a.frames);L.mg_prof(0)
L.mg_hist.restype=ctypes.POINTER(ctypes.c_uint64);hist=L.mg_hist();N=0x100000//16+0x8000//16;total=sum(hist[i] for i in range(N));rows=[]
for addr,size,name in syms:
 addr&=~1
 if 0x08800000<=addr<0x08900000:base=(addr-0x08800000)//16;end=min((addr+size-0x08800000+15)//16,0x100000//16)
 elif 0x03000000<=addr<0x03008000:base=0x100000//16+(addr-0x03000000)//16;end=min(base+(size+15)//16,N)
 else:continue
 cost=sum(hist[i] for i in range(base,end))
 if cost:rows.append({'symbol':name,'cycles_approx':cost,'percent_approx':100*cost/total})
rows.sort(key=lambda r:r['cycles_approx'],reverse=True)
out={'core':'mGBA 0.10.5','display_frame_window':a.frames,'sampled_cycles':total,'limitations':'16-byte bins overlap function boundaries; approximate function attribution, not disjoint totals','functions':rows[:30]}
(O/('R46_'+a.label+'_MGBA_HOT.json')).write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
