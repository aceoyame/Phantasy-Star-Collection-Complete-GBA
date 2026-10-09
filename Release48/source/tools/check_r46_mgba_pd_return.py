#!/usr/bin/env python3
"""Native debug PD1 entrance, actual RUN input, then Piata field/music load."""
from pathlib import Path
import json,sys,array,wave,argparse
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_mgba'));from mgba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP16');args=ap.parse_args();prefix='R46_'+args.label
g=GBA(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba');g.load(O/(prefix+'_MGBA_DEBUG.bin'))
def begin(name,args=()):
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,v in enumerate(args):L.mg_set_reg(i,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.mg_set_reg(i,v)
def call(name,args=()):
 begin(name,args);assert run_until_break(g,0x08800000,300),name
def mem(p,n):return bytes(g.rd8(p+i) for i in range(n))
def diag():
 d=s['ps4_audio_diag'];return dict(zip(['vblanks','productions','underruns','late_commits'],[g.rd32(d+i*4) for i in range(4)]))
call('ps4_debug_level60_party');g.wr8(s['ps4_menu_overlay_active'],0)
persistent=mem(0x0200f100,96)+mem(0x0200f410,44);before=diag();f=g.frame()
# The tiny debug wrapper is inlined in this build. Supply its two fields and
# call its unchanged native battle_run_random continuation.
g.wr8(s['battle_debug_enemy_pending'],0x85);g.wr8(s['battle_debug_battle_active'],1)
begin('battle_run_random');assert run_until_break(g,s['battle_choose_main_option'],6000),'PD1 battle never reached commands'
L.mg_break(0xffffffff);assert run_until_break(g,s['battle_ui_wait_edge'],1000)
L.mg_break(0xffffffff);g.press('UP',30,30) # source option 0 -> RUN (2)
L.mg_break(s['battle_exit_debug_test']);g.press('A',30,30)
if not L.mg_hit():g.run(1000)
assert L.mg_hit(),'RUN did not reach debug exit'
L.mg_break(0x08800000);g.run(60);g.press('A',30,30)
if not L.mg_hit():g.run(1000)
assert L.mg_hit(),'debug exit did not return'
assert g.reg(0)==1 and persistent==mem(0x0200f100,96)+mem(0x0200f410,44),'debug RUN returned wrong code or changed persistent rewards'
g.wr8(s['battle_debug_enemy_pending'],255);g.wr8(s['battle_debug_battle_active'],0)
battle={'display_frames':g.frame()-f,'audio_delta':{k:v-before[k] for k,v in diag().items()},'debug_RUN_return':1,'persistent_rewards_unchanged':True}
assert battle['audio_delta']['underruns']==0,battle
# Debug RUN returns to its field caller. Continue through the actual native
# Piata loader, rather than a fabricated framebuffer or audio-only restart.
g.wr8(s['ps4_debug_autoplace_pending'],1);begin('field_runtime_loop',(0x13,0x110,0x120,2));assert run_until_break(g,s['field_runtime_active'],1800)
L.mg_break(0xffffffff);g.run(30);before=diag();f=g.frame();n=g.rd16(s['native_main_frame_count']);pcm=O/(prefix+'_MGBA_PD_RETURN.pcm');g.pcm(pcm);g.run(300);g.pcm()
data=pcm.read_bytes();samples=array.array('h');samples.frombytes(data);assert any(samples),'Piata output is silent'
with wave.open(str(pcm.with_suffix('.wav')),'wb') as w:w.setnchannels(2);w.setsampwidth(2);w.setframerate(32768);w.writeframes(data)
pcm.unlink();field={'display_frames':g.frame()-f,'native_updates':(g.rd16(s['native_main_frame_count'])-n)&65535,'audio_delta':{k:v-before[k] for k,v in diag().items()},'sound_index':g.rd8(0x0200500a),'nonzero_PCM':True}
assert field['audio_delta']['underruns']==0 and field['audio_delta']['productions']==300,field
field['current_music_id']=g.rd8(s['current_music_id'])
assert field['current_music_id']==0x84,'Piata soundtrack was not restored'
g.shot(str(O/(prefix+'_MGBA_PD_RETURN.png')),2)
result={'core':'upstream mGBA 0.10.5','PD1_native_debug_battle':battle,'Piata_native_field_continuation':field,'limits':'Debug RUN plus controlled actual field load; no natural underrun reproduced, no normal-save escape odds, no audible or physical-device certification'}
(O/(prefix+'_MGBA_PD_RETURN.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
