#!/usr/bin/env python3
"""Matching-ROM controlled field warps and real IRQ starvation/recovery.

Uses the actual native map loader, field loop and audio transport. This does
not assert normal-save story progression, audible quality or physical timing.
"""
from pathlib import Path
import argparse,json,sys,wave,array,math
SRC=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--project',type=Path,default=SRC.parent);ap.add_argument('--label',default='CP16');a=ap.parse_args()
R=a.project/'source';O=a.project/'build_artifacts';O.mkdir(exist_ok=True)
sys.path.insert(0,str(SRC/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
prefix='R46_'+a.label;g=GBA(a.project/'binaries/RUN_THIS_RELEASE48_PSC.gba')
state=O/(prefix+'_MGBA_DEBUG.bin');assert state.exists(),state
def begin(name,args=()):
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.mg_set_reg(i,v)
def call(name,args=()):
 begin(name,args);assert run_until_break(g,0x08800000,300),name
def diag():
 d=s['ps4_audio_diag'];return dict(zip(['vblanks','productions','underruns','late_commits'],[g.rd32(d+i*4) for i in range(4)]))
def delta(b):return {k:v-b[k] for k,v in diag().items()}
def xy():return [g.rd32(s['chaz']),g.rd32(s['chaz']+4)]
rows=[]
for mapid in [0x13,*range(0x100,0x109),*range(0x199,0x1a0)]:
 g.load(state);call('ps4_debug_level60_party');g.wr8(s['ps4_menu_overlay_active'],0);g.wr8(s['ps4_debug_autoplace_pending'],1)
 begin('field_runtime_loop',(mapid,0x110,0x120,2))
 assert run_until_break(g,s['field_runtime_active'],1800),('field initialization',hex(mapid),hex(g.reg(15)))
 L.mg_break(0xffffffff)
 for keys in [0,16]:
  b=diag();f=g.frame();n=g.rd16(s['native_main_frame_count']);pos=xy();g.run(300,keys)
  actual=int.from_bytes(bytes([g.rd8(0x0200ec28),g.rd8(0x0200ec29)]),'big')
  row={'map':mapid,'input':'RIGHT' if keys else 'stationary','display_frames':g.frame()-f,'native_updates':(g.rd16(s['native_main_frame_count'])-n)&65535,'xy_before':pos,'xy_after':xy(),'map_after':actual,'audio_delta':delta(b)}
  assert row['audio_delta']['underruns']==0,row
  rows.append(row);print(json.dumps(row),flush=True)
  g.shot(str(O/(prefix+f'_MGBA_FIELD_{mapid:03X}_{keys}.png')),2)
 if mapid==0x13:
  # Fault injection skips only eight producer calls at their actual entry.
  # Timer1, FIFO DMA and the real commit/starvation path remain active.
  pcm=O/(prefix+'_MGBA_AUDIO_RECOVERY.pcm');g.pcm(pcm)
  g.run(30);b=diag()
  for fault in range(8):
   assert run_until_break(g,s['audio_irq_produce'],100)
   L.mg_set_reg(15,g.reg(14)&~1)
  starved=delta(b);assert starved['underruns']>0,starved
  L.mg_break(0xffffffff);g.run(10);b=diag();g.run(300);recovered=delta(b)
  assert abs(recovered['productions']-300)<=1 and recovered['underruns']==0,recovered
  g.pcm();data=pcm.read_bytes();samples=array.array('h');samples.frombytes(data)
  tail=samples[-32768*2:];rms=math.sqrt(sum(x*x for x in tail)/len(tail));assert rms>1,'Recovered PCM is silent'
  with wave.open(str(pcm.with_suffix('.wav')),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(32768);w.writeframes(data)
  pcm.unlink();recovery={'fault':'eight native producer calls skipped at entry; Timer1 consumption IRQ and FIFO DMA remain active','starved':starved,'recovered_300_frames':recovered,'last_second_pcm_rms':rms,'audible_quality_checked':False}
  print(json.dumps(recovery),flush=True)
(O/(prefix+'_MGBA_FIELD_AUDIO.json')).write_text(json.dumps({'core':'upstream mGBA 0.10.5','matching_debug_state':str(state.name),'fixtures':'controlled native map warps; default story flags; movement can meet walls, seams or events','field':rows,'recovery':recovery,'normal_save_or_hardware_validation':False},indent=2)+'\n')
