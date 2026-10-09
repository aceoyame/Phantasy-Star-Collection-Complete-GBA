#!/usr/bin/env python3
"""All 52 music IDs in the real stationary field loop with live audio IRQs."""
from pathlib import Path
import argparse,json,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);ap.add_argument('--frames',type=int,default=300);a=ap.parse_args();prefix='R46_'+a.label
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');state=O/(prefix+'_MGBA_DEBUG.bin')
def begin(name,args=(),stack=0x03007d80):
 address=s[name];L.mg_break(0xffffffff);L.mg_cpsr(0x3f if address&1 else 0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,stack),(14,0x08800000),(15,address&~1)]:L.mg_set_reg(i,v)
def call(name,args=(),stack=0x03007d80):
 begin(name,args,stack);assert run_until_break(g,0x08800000,300),name
def diag():
 p=s['ps4_audio_diag'];return dict(zip(['vblanks','productions','underruns','late_commits'],[g.rd32(p+4*i) for i in range(4)]))
rows=[]
for id in range(0x81,0xb5):
 g.load(state);call('ps4_debug_level60_party');g.wr8(s['ps4_menu_overlay_active'],0);g.wr8(s['ps4_debug_autoplace_pending'],1)
 begin('field_runtime_loop',(0x13,0x110,0x120,2));assert run_until_break(g,s['field_runtime_active'],1800)
 L.mg_break(0xffffffff);g.run(30)
 assert run_until_break(g,s['waitvb'],1000)
 registers=[g.reg(i) for i in range(15)];assert registers[13]>0x03007500
 caller_stack=g.mem(0x03007500,0x900)
 call('ps4_audio_sound_index',(id,),stack=0x03007400);assert g.reg(0)==1
 assert g.mem(0x03007500,0x900)==caller_stack
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,value in enumerate(registers):L.mg_set_reg(i,value)
 L.mg_set_reg(15,s['waitvb']);g.run(3)
 p=s['ps4_audio_diag'];g.wr16(p+26,0)
 before=diag();g.run(a.frames);delta={k:v-before[k] for k,v in diag().items()}
 # Display-frame capture can include one additional boundary while an IRQ
 # finishes; production is now tied to Timer1 rather than display VCount.
 assert abs(delta['productions']-a.frames)<=1 and delta['underruns']==delta['late_commits']==0,(id,delta)
 row={'music_id':id,'display_frames':a.frames,'audio_delta':delta,'max_produce_Timer3_ticks':g.rd16(p+26),'max_produce_CPU_cycles':g.rd16(p+26)*64}
 rows.append(row);print(json.dumps(row),flush=True)
(O/(prefix+'_MGBA_MUSIC_DEADLINES.json')).write_text(json.dumps({'core':'upstream mGBA 0.10.5','songs':rows,'live_audio_IRQs':True,'scope':'52 controlled music requests in actual stationary Piata loop; first 300 frames of each music ID; no full-song, natural scene/PD, listening or physical-device certification'},indent=2)+'\n')
