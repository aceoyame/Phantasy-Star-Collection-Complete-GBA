#!/usr/bin/env python3
"""CP28 actual mixed rounds reaching the compiled turn-end handler."""
from pathlib import Path
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='CP28'"),str(h),'exec'),globals())
def rotate(v):return(((v>>17)|((v&0x10000)>>1))<<16)|(v&65535)
cases=[(8,100,8,100,1),(0x20,100,0x20,100,1),(0x28,100,0x28,100,1),
 (0x2a,100,0x2a,100,4),(0x0c,0,0x2a,100,1),(0x48,0,0x2a,100,1),
 (8,100,2,100,1),(8,100,8,0,1),(0x80|8,100,0x40|8,100,1),
 (2,100,2,100,1),(2,100,2,0,1),(0x28,100,0x2a,100,0)]
for idx,(pst,php,est,ehp,en) in enumerate(cases):
 fixture(1,[(0,php,pst)])
 # Guard all five actor/four enemy structures, including inactive slots.
 g.put(pp+62,bytes([0xa5])*248)
 for i in range(4):
  call('battle_load_enemy',(9,ee+i*62));g.wr16(ee+i*62,ehp);g.wr8(ee+i*62+27,est)
 for addr in [pp]+[ee+i*62 for i in range(4)]:
  g.wr8(addr+25,1);g.wr8(addr+26,2);g.wr8(addr+32,79);g.wr8(addr+33,93)
  g.wr8(addr+34,1);g.wr8(addr+48,4)
 g.wr32(symbols['battle_enemy_count'],en);g.put(cp,bytes([5,0,0,0])*5);g.wr32(tp,0)
 p=bytearray(g.mem(pp,310));e=bytearray(g.mem(ee,248));saved=g.mem(md+0xf100,0xa00)
 seed=int.from_bytes(g.mem(md+0xef0c,4),'big');rolls=int(bool(pst&0x28))+en*int(bool(ehp and est&0x28))
 for _ in range(rolls):seed=rotate(seed)
 call('battle_resolve_mixed_round',(pp,1,ee,cp,0xffffffff,tp))
 for kind,addr,old,n in [('party',pp,p,1),('enemy',ee,e,en)]:
  got=g.mem(addr,len(old))
  for i in range(len(old)//62):
   k=i*62;live=i<n and(kind=='party' or int.from_bytes(old[k:k+2],'little')>0)
   st=old[k+27];options=[]
   for wake in ([0,1] if live and st&0x28 else [0]):
    want=bytearray(old[k:k+62])
    if live:
     want[34]=want[48]
     if wake:want[27]&=0xd7;want[25]=want[32]
     if kind=='enemy':want[27]&=0xfd
    options.append(bytes(want))
   assert got[k:k+62] in options,(idx,kind,i,got[k:k+62].hex(),[v.hex() for v in options])
 assert g.mem(md+0xf100,0xa00)==saved,(idx,'saved RAM')
 assert g.mem(md+0xef0c,4)==seed.to_bytes(4,'big'),(idx,'battle RNG stream',g.mem(md+0xef0c,4).hex(),f'{seed:08x}')
 results.append({'case':idx,'party_status':pst,'enemy_status':est,'party_HP':php,'enemy_HP':ehp,'occupied_native_enemy_count':en,
 'RNG2_calls':rolls,'route':'actual compiled battle_resolve_mixed_round; all fixture actors incapacitated',
 'all_actor_and_saved_bytes_guarded':True,'both_source_wake_outcomes_accepted_without_reconstructing_hardware_clock':True})
 print('PASS TURN_END',idx,flush=True)
report={'core':'upstream mGBA 0.10.5','matching_ROM_state_crc_verified':True,'cases':results,'native_cases':len(results),
 'source_RNG2_stream_rotations_and_no_initiative_target_selection_RNG1_calls':True,
 'source_sleep_flags_AGI_only_physical_restore_paralysis_bit_only_and_inactive_guards':True,
 'normal_playthrough_controller_audio_quality_physical_persistence_verified':False}
(O/(prefix+'_MGBA_TURN_END.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'actual mixed-round turn-end cases',flush=True)

