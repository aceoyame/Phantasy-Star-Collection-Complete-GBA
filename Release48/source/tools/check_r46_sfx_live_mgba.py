#!/usr/bin/env python3
"""Controlled source sound commands during the real field loop with live IRQs."""
from pathlib import Path
import argparse,json,sys
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
ap=argparse.ArgumentParser();ap.add_argument('--label',required=True);a=ap.parse_args();prefix='R46_'+a.label
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
mask_address=next(v for k,v in s.items() if k.endswith('::render_mute_mask'))
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');state=O/(prefix+'_MGBA_DEBUG.bin')
def begin(name,args=(),stack=0x03007d80):
 address=s[name];L.mg_break(0xffffffff);L.mg_cpsr(0x3f if address&1 else 0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,stack),(14,0x08800000),(15,address&~1)]:L.mg_set_reg(i,v)
def call(name,args=(),stack=0x03007d80):
 begin(name,args,stack);assert run_until_break(g,0x08800000,300),name
def field_command(id):
 # Pause at an ARM function entry, where condition flags are not arguments.
 assert run_until_break(g,s['waitvb'],1000),'field did not reach waitvb'
 registers=[g.reg(i) for i in range(15)];assert registers[13]>0x03007500
 caller_stack=g.mem(0x03007500,0x900)
 call('ps4_audio_sound_index',(id,),stack=0x03007400);assert g.reg(0)==1
 assert g.mem(0x03007500,0x900)==caller_stack,'sound fixture overwrote live caller stack'
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,value in enumerate(registers):L.mg_set_reg(i,value)
 L.mg_set_reg(15,s['waitvb'])
def diag():
 p=s['ps4_audio_diag'];return dict(zip(['vblanks','productions','underruns','late_commits'],[g.rd32(p+i*4) for i in range(4)]))
def span(frames):
 before=diag();n=g.rd16(s['native_main_frame_count']);g.run(frames)
 delta={k:v-before[k] for k,v in diag().items()}
 assert delta['underruns']==0 and delta['late_commits']==0,delta
 assert delta['productions']>=frames-1,delta
 return {'display_frames':frames,'native_updates':(g.rd16(s['native_main_frame_count'])-n)&65535,'audio_delta':delta}
rows=[]
for sid in range(0xf8,0xfb):
 for nid in [0xbd,0xf2]: # FM4+FM5 healing cue; FM5-only cursor cue.
  g.load(state);call('ps4_debug_level60_party');g.wr8(s['ps4_menu_overlay_active'],0);g.wr8(s['ps4_debug_autoplace_pending'],1)
  begin('field_runtime_loop',(0x13,0x110,0x120,2));assert run_until_break(g,s['field_runtime_active'],1800)
  L.mg_break(0xffffffff);g.run(30)
  field_command(0x95);field_command(sid);assert g.rd16(mask_address)==16
  first=span(120);field_command(nid);assert g.rd16(mask_address)==48
  overlap=span(60);assert not g.rd8(s['normal_active']) and g.rd8(s['special_active']) and g.rd16(mask_address)==16
  field_command(0xfc);assert g.rd16(mask_address)==16
  resumed=span(120);field_command(0xfd);assert g.rd16(mask_address)==0
  stopped=span(120);assert g.rd8(s['current_music_id'])==0x95
  row={'special_id':sid,'normal_id':nid,'special_only':first,'ordinary_overlap_and_expiry':overlap,'after_StopSFX':resumed,'after_StopSpcSFX':stopped}
  rows.append(row);print(json.dumps(row),flush=True)
(O/(prefix+'_MGBA_SFX_LIVE.json')).write_text(json.dumps({'core':'upstream mGBA 0.10.5','cases':rows,'live_IRQs':True,'scope':'actual stationary Piata loop and source-sequenced combat music with controlled sound requests at ARM waitvb entries; no natural vehicle travel, audible quality, PD or physical-device certification'},indent=2)+'\n')
