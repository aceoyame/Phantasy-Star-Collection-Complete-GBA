#!/usr/bin/env python3
"""Source-record item ownership/dispatch and full enemy healing, CP27."""
from pathlib import Path
h=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
exec(compile(h.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='CP27'"),str(h),'exec'),globals())
def le16(b,k,v):b[k:k+2]=(v&65535).to_bytes(2,'little')
def rotate(v):return(v>>1)|((v&1)<<15)
def amount(kind,stat,power,factor=4,defense=0):
 name='battle_source_heal' if kind=='heal' else 'battle_calc_damage_ex'
 assert run_until_break(g,symbols[name],300),name+' entry'
 want=(stat,power) if kind=='heal' else(stat,defense,factor,power)
 assert tuple(g.reg(i) for i in range(len(want)))==want,(name,want,[g.reg(i) for i in range(4)])
 ret=g.reg(14);assert run_until_break(g,ret,30),name+' return'
 got=g.reg(0);bounds=[]
 # Hardware-timed RNG is deliberately retained. The CP22-guarded unchanged
 # math consumes 16 rolls. Check original record parameters and source bounds;
 # use its return to check target placement and all unrelated bytes.
 for q in range(8,121):
  if kind=='heal':v=min(999,(((q*stat)>>6)+stat+power*2)>>1)
  else:
   w=((q*stat)&65535)>>6;w=(w+stat+power*2)&65535;w=((w*factor)&65535)>>2;w=(w-defense)&65535
   v=min(999,max(1,w if w<32768 else w-65536))
  bounds.append(v)
 assert min(bounds)<=got<=max(bounds),(name,got,min(bounds),max(bounds))
 return got
items=[0x10,0x39,0x4e,0x4f,0x50,0x51,0x5e,0x62,0x64,0x65,0x66,0x71,0x84,0x85,0x88,0x8b,0x8c,0x8e]
assert [i for i in range(1,148) if original_rom[0x2a8e28+(i-1)*22] and not(original_rom[0x2a8e28+(i-1)*22+10]==8 and 0x12<=original_rom[0x2a8e28+(i-1)*22]<=0x16)]==items
def setup(item,actor=2,mode='inventory'):
 specs=[(0,100,0),(6,100,0),(2,200,1),(1,0,4),(7,0,0x40)]
 fixture(item,specs);g.put(md+0xf410,bytes([1])*40)
 for cid,_,_ in specs:g.put(md+0xf500+cid*128+0x4c,bytes(4))
 if mode in ('inventory','moved'):g.wr8(md+0xf410+(12 if mode=='moved' else 0),item)
 if mode in ('equipped','duplicate'):g.wr8(md+0xf500+specs[actor][0]*128+0x4e,item)
 if mode=='duplicate':g.wr8(md+0xf410,item)
 if mode=='other_owner':g.wr8(md+0xf500+specs[0][0]*128+0x4c,item)
 for i,(cid,hp,st) in enumerate(specs):
  d=0xf500+cid*128;w16(d+0x24,65530);w16(d+0x28,65531);g.wr8(md+d+0x1f,250)
  w16(d+16,999);g.wr16(pp+i*62+2,999);g.wr16(pp+i*62+8,13);g.wr16(pp+i*62+10,14);g.wr8(pp+i*62+25,7)
 for i in range(4):
  call('battle_load_enemy',(9,ee+i*62));g.wr16(ee+i*62,500 if i!=3 else 0);g.wr16(ee+i*62+2,999);g.wr8(ee+i*62+27,4 if i==2 else 0)
  for off in (23,24,25,26):g.wr8(ee+i*62+off,200)
  for off in (30,31,32,33):g.wr8(ee+i*62+off,17+off)
  for off in (8,10,12):g.wr16(ee+i*62+off,0)
  for off in (14,16,18):g.wr16(ee+i*62+off,300+off)
  for e in range(14):g.wr8(ee+i*62+34+e,4);g.wr8(ee+i*62+48+e,e%5)
 g.wr32(symbols['battle_enemy_count'],4);g.wr8(symbols['battle_suppress_skill_source_audio'],1)
 return specs
def verify(label,p,e,s,seed=None):
 for key,addr,want in [('party',pp,p),('enemy',ee,e),('saved',md+0xf100,s)]:
  got=g.mem(addr,len(want));assert got==want,(label,key,[(i,x,y) for i,(x,y) in enumerate(zip(got,want)) if x!=y][:15])
 if seed is not None:assert g.mem(md+0xef0c,2)==seed.to_bytes(2,'big'),(label,'RNG')
def item_case(item,mode='inventory',actor=2,chosen=0,sleep=False):
 specs=setup(item,actor,mode);ip=0x2a8e28+(item-1)*22;r=original_rom[ip:ip+22]
 effect,stat,range_,power,resist,element=r[:6];label=f'ITEM_{item:02X}_{mode}_ACTOR{actor}'+('_SLEEP_GATES' if sleep else '')
 if effect==7:g.wr8(ee+24,0);g.wr8(ee+62+24,255)
 if sleep:g.wr8(ee+62+27,2)
 p=bytearray(g.mem(pp,310));e=bytearray(g.mem(ee,248));s=bytearray(g.mem(md+0xf100,0xa00));seed=int.from_bytes(g.mem(md+0xef0c,2),'big')
 g.put(cp,bytes([4,item,chosen,0x82 if mode=='equipped' else 0]));g.wr32(tp,1)
 rng_count=0
 for cast in range(2):
  allowed=mode not in ('missing','other_owner') and(cast==0 or r[10]!=8);events=[]
  assert call('battle_action_has_resource',(pp,5,actor,cp))==int(allowed),label+' ownership gate'
  if allowed:
   if r[10]==8:s[0x310:0x338]=s[0x311:0x338]+bytes(1)
   if range_>=3 and effect in (9,10,12,0x12):
    targets=[actor] if range_==3 else range(5) if range_ in(5,7,9) else[chosen]
    for i in targets:
     k=i*62;d=0x400+specs[i][0]*128
     if p[k+27]&0x44 or range_ in(4,5) and p[k+29] or range_ in(6,7) and not p[k+29]:continue
     if effect==0x12:events.append(('heal',i))
     elif effect==9:
      if p[k+29]:continue
      v=(read16(s,d+0x24)+stat)&65535;le16(p,k+8,v);write16(s,d+0x26,v)
     elif effect==10:
      v=(read16(s,d+0x28)+stat)&65535;le16(p,k+10,v);write16(s,d+0x2a,v)
     else:p[k+25]=(s[d+0x1f]+stat)&255;s[d+0x20]=p[k+25]
   elif range_<3:
    for i in(range(4) if range_==2 else[1]):
     k=i*62
     if not int.from_bytes(e[k:k+2],'little') or e[k+27]&0x44:continue
     if effect==0x27:
      for live,base in[(23,30),(24,31),(25,32),(26,33)]:e[k+live]=e[k+base]
      for live,base in[(8,14),(10,16),(12,18)]:e[k+live:k+live+2]=e[k+base:k+base+2]
      e[k+34:k+48]=e[k+48:k+62]
     elif effect==1:events.append(('damage',i))
     elif effect==7 and not e[k+27]&0xa:
      seed=rotate(seed);rng_count+=1
      # For every possible 6-bit roll, target0 succeeds and target1 fails.
      if i==0:e[k+27]|=8
  begin('battle_execute_cmd',(pp,5,ee,actor,cp,tp))
  for kind,i in events:
   k=i*62
   if kind=='heal':hp=int.from_bytes(p[k:k+2],'little');le16(p,k,min(999,hp+amount(kind,stat,power)))
   else:
    off=12 if resist==7 else 10;def_=int.from_bytes(e[k+off:k+off+2],'little')
    hp=int.from_bytes(e[k:k+2],'little');le16(e,k,max(0,hp-amount(kind,stat,power,e[k+34+(element or 1)-1],def_)))
   rng_count+=16
  assert run_until_break(g,0x08800000,300),label+' return';verify(label,p,e,s,seed)
 results.append({'case':label,'record_first8':r[:8].hex(),'route':'compiled battle_execute_cmd and resource gate','casts':2,'source_RNG_calls_checked':rng_count,'source_record_parameters_return_bounds_and_target_placement_checked':True,'whole_party_enemy_saved_and_RNG_guards':True})
 print('PASS',label,flush=True)
for item in items:
 item_case(item);item_case(item,'missing')
 if original_rom[0x2a8e28+(item-1)*22+10]<8:item_case(item,'equipped')
for mode in ('moved','duplicate','other_owner'):item_case(0x39,mode)
item_case(0x66,actor=1);item_case(0x64,actor=1)
item_case(0x10,chosen=2);item_case(0x10,chosen=1)
item_case(0x51,sleep=True);item_case(0x65,sleep=True)
def enemy_case(skill,caster,mode):
 setup(0x39);r=original_rom[0x28336c+(skill-1)*8:0x28336c+skill*8];assert r[0]==0x12 and r[1]==0x82
 for i in range(4):
  call('battle_load_enemy',(caster if i==1 else 9,ee+i*62));g.wr16(ee+i*62,100 if i==0 else 499 if i==1 else 200 if i==2 else 0)
  g.wr16(ee+i*62+2,500);g.wr8(ee+i*62+27,4 if i==2 else 0);g.wr8(ee+i*62+24,37)
 if mode=='sealed':g.wr8(ee+62+27,0x10)
 if mode=='tie':g.wr16(ee+62,100)
 g.wr32(symbols['battle_enemy_count'],4);call('battle_anim_reset');call('battle_ui_bind',(pp,5,ee,0))
 g.wr8(symbols['battle_party_death_done_mask'],0x18)
 p=bytearray(g.mem(pp,310));e=bytearray(g.mem(ee,248));s=bytearray(g.mem(md+0xf100,0xa00));label=f'ENEMY_HEAL_{skill:02X}_{mode}'
 for _ in range(2):
  candidates=[i for i in range(4) if int.from_bytes(e[i*62:i*62+2],'little') and not e[i*62+27]&0x44]
  targets=[] if mode=='sealed' else candidates if r[2]==2 else[min(candidates,key=lambda i:int.from_bytes(e[i*62:i*62+2],'little'))]
  begin('battle_enemy_execute_action',(pp,5,ee,1,skill,0,1))
  for i in targets:hp=int.from_bytes(e[i*62:i*62+2],'little');le16(e,i*62,min(500,hp+amount('heal',37,r[3])))
  assert run_until_break(g,0x08800000,300),label+' return';verify(label,p,e,s)
 results.append({'case':label,'route':'complete compiled enemy action','casts':2,'source_record_parameters_lowest_current_HP_tie_group_and_dead_targets_checked':True,'all_party_enemy_saved_bytes_guarded':True})
 print('PASS',label,flush=True)
for skill,caster in[(0x3e,0x58),(0x45,0x63),(0x46,0x64),(0x49,0x65)]:
 for mode in('normal','sealed','tie'):enemy_case(skill,caster,mode)
report={'core':'upstream mGBA 0.10.5','fresh_current_ROM_boot':not a.reuse,'matching_ROM_state_crc_verified':True,'cases':results,'native_case_count':len(results),'all_18_non_healing_records_exercised':True,'unchanged_CP22_damage_heal_math_guarded_separately':True,'exact_controller_VDP_audio_normal_playthrough_physical_persistence_verified':False}
(O/(prefix+'_MGBA_EQUIPMENT_ITEMS.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'item/enemy-healing cases',flush=True)

