#!/usr/bin/env python3
"""Fresh upstream mGBA timing/audio diagnostics; controlled native actor fixtures.
No ROM hooks, source updates dropped, or imported HLE states. Not a normal-save
playthrough or a physical-device/Genesis VDP certification.
"""
from pathlib import Path
import argparse,json,sys,wave
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_mgba'));import mgba
sys.modules['gba']=mgba
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'));from boot_debug import boot_to_debug
from mgba import GBA,L,run_until_break
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP16');ap.add_argument('--reuse',action='store_true');a=ap.parse_args();prefix='R46_'+a.label
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba';state=O/(prefix+'_MGBA_DEBUG.bin')
if a.reuse:g=GBA(rom);g.load(state)
else:g=boot_to_debug(str(rom),str(state))
print('Fresh mGBA module boot reached boss selector',flush=True)
def mem(p,n):return bytes(g.rd8(p+i) for i in range(n))
def begin(name,args=()):
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,v in enumerate(args[:4]):L.mg_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.mg_set_reg(i,v)
def diag():
 d=s['ps4_audio_diag'];return dict(zip(['vblanks','productions','underruns','late_commits','waitvb_calls','commands'],[g.rd32(d+i*4) for i in range(6)]))
def call(name,args=(),limit=300):
 begin(name,args);assert run_until_break(g,0x08800000,limit),(name,hex(g.reg(15)))
# Idle combo-track arena; PD owns $D000-$DFFF while its controller runs.
pp,ee=0x02033600,0x02033800;cases=[]
for id,skill,ticks in [(0x85,0,192),(0x85,0x67,519),(0x86,0x68,595)]:
 g.load(state)
 # Existing debug party initialization gives the requested real five actors.
 call('ps4_debug_level60_party')
 ids=[g.rd8(0x0200f40a+i) for i in range(5)]
 for i,cid in enumerate(ids):call('battle_load_char',(cid,pp+i*62))
 call('battle_load_enemy',(id,ee));g.wr32(s['battle_enemy_count'],1);g.wr8(s['battle_event_index'],1);g.wr8(s['battle_enemy_placement'],0x14)
 call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0));call('r45_pd_visual_reset',(ee,));call('r45_pd_body_snapshot',(id,))
 call('battle_compose_igg',(pp,5,ee,0,0));call('battle_present_page0');actor=mem(pp,310)
 path=O/(prefix+f'_MGBA_PD{id-0x84}_{"TRANSFORM" if skill else "OPEN"}.pcm');g.pcm(path)
 name='r45_pd_transform' if skill else 'r45_pd_opening';args=(pp,5,ee,0,skill) if skill else (pp,5,ee)
 before=diag();f=g.frame();c=g.cycles();begin(name,args)
 L.mg_break(0x08800000);sample=[]
 for n in range(16000):
  g.run(1)
  if L.mg_hit():break
  if n in [0,63,191,300,500]:
   shot=prefix+f'_MGBA_PD{id-0x84}_{skill}_{n}.png';g.shot(str(O/shot),2);sample.append(shot)
 else:raise AssertionError('Controller did not return')
 g.pcm();after=diag();frames=g.frame()-f;cycles=g.cycles()-c
 assert mem(pp,310)==actor,'PD controller changed party resources'
 assert g.rd8(ee+20)==(id+1 if skill else id),'source form callback failed'
 d={k:after[k]-before[k] for k in before};row={'form':id,'skill':skill,'party_ids':ids,'source_ticks':ticks,'display_frames':frames,'cycles':cycles,'audio_delta':d,'samples':sample,'resources_unchanged':True}
 cases.append(row);print(json.dumps(row),flush=True)
 L.mg_break(0xffffffff)
 # Audio samples produced at mGBA's SOUNDBIAS-selected rate (default 32768 Hz).
 with wave.open(str(path.with_suffix('.wav')),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(32768);w.writeframes(path.read_bytes())
 path.unlink()
(O/(prefix+'_MGBA_PD_PROFILE.json')).write_text(json.dumps({'core':'Upstream mGBA 0.10.5, software renderer, default emulated BIOS, idle optimization disabled','fresh_matching_ROM_boot':not a.reuse,'fixtures':'Native controller calls with actual initialized level-60 five-character records; no patched ROM','cases':cases,'normal_save_hardware_or_Genesis_parity':False},indent=2)+'\n')
