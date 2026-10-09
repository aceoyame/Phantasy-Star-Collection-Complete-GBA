#!/usr/bin/env python3
"""Current-ROM full enemy actions: source buffs, Reinforce, Canceling and stat down."""
from pathlib import Path
import re
helpers=Path(__file__).with_name('check_r46_special_recovery_mgba.py')
base=helpers.read_text().split('\ndef special_fixture(')[0].replace("default='CP23'","default='CP26'")
exec(compile(base,str(helpers),'exec'),globals())
def le16(buf,off,v):buf[off:off+2]=(v&65535).to_bytes(2,'little')
records={
 0x1d:('Barrier',0x30,'0b01020000000000'),0x26:('Shift',0x40,'0982030000000000'),
 0x27:('Saner',0x41,'0c82020000000000'),0x2d:('Deban',0x46,'0a82020000000000'),
 0x62:('Reinforce',0x80,'2b00030000000000'),0x69:('Canceling',0x87,'2700090000000000'),
 0x10:('Thread',0x1f,'0601084003010000'),0x16:('Flash',0x2b,'21010940020b0000'),
 0x28:('Doran',0x40,'06820950020b0000'),0x4b:('ShadowBind',0x69,'06020940020b0000'),
 0x57:('Gelun',0x73,'03820940020b0000')}
for id_,(name,caster,hx) in records.items():
 assert original_rom[0x28336c+(id_-1)*8:0x28336c+id_*8].hex()==hx,(id_,name)
 assert id_ in original_rom[0x2816bc+caster*48+32:0x2816bc+caster*48+44],name
# Constants and source define a full $00..$70 table; never scan subsequent data.
constants=(S/'references/disassembly/ps4disasm/ps4disasm/ps4.constants.asm').read_text()
assert 'EnemySkillID_BlackWave3 = id(EnemySkill_BlackWave3)\t; $70' in constants
assert [i for i in range(1,0x71) if original_rom[0x28336c+(i-1)*8] in (3,4,6,0x21)]==[0x10,0x16,0x28,0x4b,0x57]
def setup(id_,specs=None):
 specs=specs or [(0,100,0),(6,100,0),(1,100,4),(7,100,0x40),(8,0,0)]
 fixture(1,specs)
 for i,(cid,hp,st) in enumerate(specs):
  d=0xf500+cid*128
  for off,v in [(0x19,255-i),(0x1c,i),(0x1f,100+i),(0x22,110+i)]:
   g.wr8(md+d+off,v)
  for off,v in [(0x24,400+i),(0x28,500+i),(0x2c,600+i)]:w16(d+off,v)
  for off in (23,24,25,26):g.wr8(pp+i*62+off,201+off)
  for off in (8,10,12):g.wr16(pp+i*62+off,2000+off)
  for e in range(14):
   g.wr8(md+d+0x30+2*e,2);g.wr8(md+d+0x31+2*e,(i+e)%5)
   g.wr8(pp+i*62+34+e,2);g.wr8(pp+i*62+48+e,9)
 for i in range(4):
  call('battle_load_enemy',(records[id_][1] if i==1 else 9,ee+i*62))
  g.wr16(ee+i*62,100 if i!=3 else 0)
  g.wr16(ee+i*62+2,100)
  g.wr8(ee+i*62+27,4 if i==2 else 0)
 g.wr32(symbols['battle_enemy_count'],4)
 call('battle_anim_reset');call('battle_ui_bind',(pp,len(specs),ee,0))
 call('battle_compose_igg',(pp,len(specs),ee,0,0));call('battle_present_page0')
 return specs
def run_action(id_,pn,victim=0):
 call('battle_enemy_execute_action',(pp,pn,ee,1,id_,victim,1))
def verify(name,want_party,want_enemy,want_saved):
 # The complete action also runs the existing death completion callback:
 # synthetic HP0/status0 humans become dead after the stat handler runs.
 for k in range(0,len(want_party),62):
  if int.from_bytes(want_party[k:k+2],'little')==0 and not want_party[k+27]&0x44:
   want_party[k+27]=(want_party[k+27]&0x10)|(0x40 if want_party[k+29] else 4)
 for what,got,want in [('party',g.mem(pp,len(want_party)),want_party),('enemies',g.mem(ee,len(want_enemy)),want_enemy),('saved',g.mem(md+0xf100,0xa00),want_saved)]:
  if got!=want:
   differences=[(i,a,b) for i,(a,b) in enumerate(zip(got,want)) if a!=b]
   raise AssertionError((name,what,differences[:20]))
def support(id_,wrap=False,sealed=False,repeat=True):
 label=records[id_][0]+('_WRAP' if wrap else '')+('_SEALED' if sealed else '')
 specs=setup(id_)
 for i in range(4):
  a0=ee+i*62
  for off,v in [(23,37),(24,73),(25,250),(26,251),(30,17),(31,19),(32,250 if wrap else 17),(33,21)]:
   g.wr8(a0+off,v)
  for off,v in [(8,65530 if wrap else 301),(10,65531 if wrap else 302),(12,65532 if wrap else 303),(14,65530 if wrap else 101),(16,65531 if wrap else 102),(18,65532 if wrap else 103)]:
   g.wr16(a0+off,v)
 if sealed:g.wr8(ee+62+27,0x10)
 party=bytearray(g.mem(pp,len(specs)*62));want=bytearray(g.mem(ee,4*62));saved=bytearray(g.mem(md+0xf100,0xa00))
 effect=original_rom[0x28336c+(id_-1)*8];stat=37 if id_==0x1d else 73
 blocked=sealed and id_ in (0x26,0x27,0x2d)
 for cast in range(2 if repeat else 1):
  if not blocked:
   for i in range(4):
    if want[i*62+27]&0x44 or (id_ in (0x26,0x62) and i!=1):continue
    k=i*62
    if id_==0x62:
     for off in (23,24,25,26):want[k+off]=(want[k+off]+20)&255
     for off in (8,10,12):le16(want,k+off,int.from_bytes(want[k+off:k+off+2],'little')+20)
    elif id_==0x27:want[k+25]=(want[k+32]+stat)&255
    else:
     live,baseoff={0x1d:(12,18),0x26:(8,14),0x2d:(10,16)}[id_]
     le16(want,k+live,int.from_bytes(want[k+baseoff:k+baseoff+2],'little')+stat)
  run_action(id_,len(specs));verify(label,party,want,saved)
 results.append({'case':label,'route':'complete compiled battle_enemy_execute_action','casts':2 if repeat else 1,'source_record':records[id_][2],'all_actor_and_saved_bytes_guarded':True,'target_HP_zero_status_zero_included':id_ in (0x1d,0x27,0x2d),'sealed_record_gate_checked':sealed})
 print('PASS',label,flush=True)
def restore(sealed=False,feeve=False):
 specs=setup(0x69);label='CANCELING'+('_SEALED' if sealed else '')+('_AFTER_FEEVE' if feeve else '')
 if sealed:g.wr8(ee+62+27,0x10)
 if feeve:
  # Feeve's authored odd $39 WATER restore write must survive restoration.
  call('battle_apply_support_ability',(pp,len(specs),0,0,0x0d,73,0x0b,9))
 party=bytearray(g.mem(pp,len(specs)*62));enemy=bytearray(g.mem(ee,248));saved=bytearray(g.mem(md+0xf100,0xa00))
 for i,(cid,hp,st) in enumerate(specs):
  if st&0x44:continue
  k=i*62;d=0x400+cid*128
  for live,sourceoff,ramlive in [(23,0x19,0x1a),(24,0x1c,0x1d),(25,0x1f,0x20),(26,0x22,0x23)]:
   party[k+live]=saved[d+sourceoff];saved[d+ramlive]=party[k+live]
  for live,sourceoff,ramlive in [(8,0x24,0x26),(10,0x28,0x2a),(12,0x2c,0x2e)]:
   v=read16(saved,d+sourceoff);le16(party,k+live,v);write16(saved,d+ramlive,v)
  for e in range(14):
   party[k+34+e]=saved[d+0x31+2*e];saved[d+0x30+2*e]=party[k+34+e]
 for _ in range(2):
  run_action(0x69,len(specs));verify(label,party,enemy,saved)
 shot(label+'_RESULT')
 results.append({'case':label,'route':'complete compiled enemy action','casts':2,'four_byte_three_word_fourteen_property_restore':True,'RAM_source_fields_differ_from_cached_base_fields':True,'HP_TP_status_inventory_base_and_enemy_bytes_guarded':True,'source_HP_zero_status_zero_target_included':True,'Feeve_odd_restore_byte_checked':feeve})
 print('PASS',label,flush=True)
def down(id_,floor=False,sealed=False):
 specs=[(0,100,0),(6,100,0),(1,100,4),(7,100,0x40),(8,0,0)]
 specs=setup(id_,specs)
 label=records[id_][0]+('_FLOOR' if floor else '')+('_SEALED' if sealed else '')
 g.wr8(ee+62+23,255 if floor else 73);g.wr8(ee+62+24,255 if floor else 37)
 if sealed:g.wr8(ee+62+27,0x10)
 effect=original_rom[0x28336c+(id_-1)*8]
 for i,(cid,hp,st) in enumerate(specs):
  d=0xf500+cid*128
  w16(d+0x24,0 if floor else 300)
  g.wr8(md+d+0x1f,0 if floor else 100);g.wr8(md+d+0x22,0 if floor else 110)
  # Target0 always succeeds, target1 always misses, slots2/3 are dead,
  # slot4 is source-eligible HP0/status0 and succeeds in all-target records.
  for off in (23,24,25):g.wr8(pp+i*62+off,255 if i==1 else 0)
  for e in range(14):g.wr8(pp+i*62+34+e,0 if i==1 else 4)
 party=bytearray(g.mem(pp,len(specs)*62));enemy=bytearray(g.mem(ee,248));saved=bytearray(g.mem(md+0xf100,0xa00))
 blocked=sealed and id_ in (0x28,0x57)
 if not blocked:
  stat=255 if floor else (73 if id_ in (0x10,0x16) else 37)
  for i in ([0] if id_==0x10 else [0,4]):
   k=i*62;d=0x400+specs[i][0]*128
   if effect==3:
    v=max(0,(0 if floor else 300)-stat);le16(party,k+8,v);write16(saved,d+0x26,v)
   else:
    live,ramlive=(25,0x20) if effect==6 else (26,0x23)
    v=max(1,(0 if floor else (100 if effect==6 else 110))-stat)
    party[k+live]=v;saved[d+ramlive]=v
 for _ in range(2):
  run_action(id_,len(specs));verify(label,party,enemy,saved)
 results.append({'case':label,'route':'complete compiled enemy action','casts':2,'success_miss_dead_android_and_HP_zero_status_zero_targets_guarded':True,'caster_selected_stat_not_hit_byte':True,'base_replacement_floor_and_RAM_write_guarded':True,'sealed_record_gate_checked':sealed})
 print('PASS',label,flush=True)
for id_ in (0x1d,0x26,0x27,0x2d,0x62):
 support(id_);support(id_,wrap=True);support(id_,sealed=True)
restore();restore(sealed=True);restore(feeve=True)
for id_ in (0x10,0x16,0x28,0x4b,0x57):
 down(id_);down(id_,floor=True);down(id_,sealed=True)
report={'core':'upstream mGBA 0.10.5','fresh_current_ROM_boot':not a.reuse,'matching_ROM_state_crc_verified':True,'cases':results,'total_full_enemy_action_runs':sum(r['casts'] for r in results),'exact_original_controller_timing_VDP_audio_quality_normal_playthrough_or_physical_persistence_verified':False}
(O/(prefix+'_MGBA_ENEMY_STATS.json')).write_text(json.dumps(report,indent=2)+'\n')
print('PASS',len(results),'cases',report['total_full_enemy_action_runs'],'full enemy actions',flush=True)
