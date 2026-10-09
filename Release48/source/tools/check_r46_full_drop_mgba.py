#!/usr/bin/env python3
"""Fresh-ROM native victory/drop input, reward uniqueness and save-menu roundtrip.

Controlled actor/formation/drop fixtures use real menus, IRQs and save APIs.
No ROM patching, imported states, or normal-save/hardware certification.
"""
from pathlib import Path
import argparse,json,sys,zlib
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP20');ap.add_argument('--reuse',action='store_true');a=ap.parse_args();prefix='R46_'+a.label
sys.path.insert(0,str(R/'tools_host/r46_mgba'));import mgba
sys.modules['gba']=mgba
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'));from boot_debug import boot_to_debug
from mgba import GBA,L,run_until_break
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
rom=R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba';state=O/(prefix+'_MGBA_DEBUG.bin');state_id=state.with_suffix('.rom_crc.json')
rom_id={'rom_bytes':rom.stat().st_size,'rom_crc32':f'{zlib.crc32(rom.read_bytes()):08x}'}
if a.reuse:
 assert json.loads(state_id.read_text())==rom_id,'debug state belongs to a different build'
 g=GBA(rom);g.load(state)
else:
 g=boot_to_debug(str(rom),str(state));state_id.write_text(json.dumps(rom_id)+'\n')
print('Matching ROM reached native debug selector',flush=True)
md=0x02000000;pp,ee=0x02033600,0x02033800
source_rom=(R/'references/roms/ps4built.bin').read_bytes()
be=lambda p,n:int.from_bytes(source_rom[p:p+n],'big')
def w16(off,v):g.put(md+off,v.to_bytes(2,'big'))
def w32(off,v):g.put(md+off,v.to_bytes(4,'big'))
def r16(off):return int.from_bytes(g.mem(md+off,2),'big')
def r32(off):return int.from_bytes(g.mem(md+off,4),'big')
def begin(name,args=()):
 L.mg_break(0xffffffff);L.mg_cpsr(0x1f)
 for i,v in enumerate(args[:4]):L.mg_set_reg(i,v)
 for i,v in enumerate(args[4:]):g.wr32(0x03007d80+i*4,v)
 for i,v in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.mg_set_reg(i,v)
def call(name,args=()):
 begin(name,args);assert run_until_break(g,0x08800000,300),name
def diag():
 p=s['ps4_audio_diag'];return dict(zip(['vblanks','productions','underruns','late_commits'],[g.rd32(p+i*4) for i in range(4)]))
def press(key,hold=3,after=30):L.mg_break(0xffffffff);g.press(key,hold,after)
def shot(tag):g.shot(str(O/(prefix+'_DROP_'+tag+'.png')),2)

def save_roundtrip():
 # Both private save/load helpers are inlined. Drive their actual callers,
 # then stop at the title fade after Continue has loaded the complete image.
 saved=[]
 for n in range(3):
  w16(0xf012,n);w16(0xec28,0x13);g.wr8(md+0xed51,0)
  begin('ps4_field_system',(0,0));g.run(80)
  press('A',after=80);press('A',after=80)
  saved.append(g.mem(md+0xf100,0xa00));shot('SRAM_SLOT_'+str(n+1))
  press('A',after=60)
  L.mg_break(0x08800000);g.press('B',3,80)
  assert L.mg_hit(),('save menu did not return',n)
  L.mg_break(0xffffffff)
 for n in range(3):
  g.put(md+0xf100,bytes([0x55])*0xa00)
  begin('gba_main');L.mg_break(s['title_pick_three']);g.run(120)
  for _ in range(10):
   if L.mg_hit():break
   g.press('START',3,120)
  assert L.mg_hit(),('Continue menu not reached',n)
  L.mg_break(0xffffffff);g.run(240)
  L.mg_break(s['title_pick_three']);g.press('A',12,240)
  assert L.mg_hit(),('save-slot menu not reached',n)
  assert g.reg(3)==0,('saved slot checksum invalid',n,g.reg(3))
  L.mg_break(0xffffffff);g.run(240)
  for _ in range(n):press('DOWN',hold=12,after=120)
  shot('CONTINUE_SLOT_'+str(n+1))
  L.mg_break(s['fade_framebuffer_to_black']);g.press('A',12,240)
  assert L.mg_hit(),('Continue did not load slot',n)
  assert g.mem(md+0xf100,0xa00)==saved[n],('SRAM roundtrip',n)
  L.mg_break(0xffffffff)
 return list(range(3))

results=[]
def fixture(tag,inventory,drop=0x4e,rate=127,debug=0,specs=None):
 g.load(state);g.wr8(s['ps4_menu_overlay_active'],0)
 w16(0xf508,1);w32(0xf50a,be(0x2856b0,4)-1)
 g.put(md+0xf552,bytes(16));g.put(md+0xf562,bytes(24))
 specs=specs or [(0,999,0,0)];pn=len(specs)
 for i,(cid,hp,status,android) in enumerate(specs):
  d=0xf500+cid*128;w16(d+6,5 if android else 0);w16(d+14,hp);w16(d+16,100)
  g.wr8(md+d+22,status);g.wr8(md+d+31,33+i);g.wr8(md+d+34,44+i)
  call('battle_load_char',(cid,pp+i*62))
 call('battle_store_party',(pp,pn))
 call('battle_load_enemy',(9,ee));g.wr16(ee,0)
 g.wr32(s['battle_enemy_count'],1);g.wr8(s['battle_enemy_reward_mask'],1)
 g.wr8(s['battle_event_index'],1);g.wr8(s['battle_debug_battle_active'],debug)
 g.put(md+0xf410,inventory);w32(0xf438,100)
 g.wr8(md+0x41f2,rate);g.wr8(md+0x41f3,drop);w16(0xef0c,0x1234)
 call('battle_anim_reset');call('battle_ui_bind',(pp,pn,ee,0));call('r45_pd_visual_reset',(ee,))
 call('battle_compose_igg',(pp,pn,ee,0,0));call('battle_present_page0')
 actors=g.mem(pp,62*pn)+g.mem(ee,62);xp=r32(0xf50a);before=diag()
 begin('battle_show_results',(ee,pp,pn));g.run(100)
 assert r32(0xf50a)==xp+(0 if debug else be(0x2816bc+9*48+44,2)//sum(bool(hp) and not(status&0x44) for _,hp,status,_ in specs)),tag
 assert r32(0xf438)==100+(0 if debug else be(0x2816bc+9*48+46,2)),tag
 assert r16(0xf508)==(1 if debug else 2),tag
 return {'tag':tag,'inventory':inventory,'drop':drop,'debug':debug,'specs':specs,'pn':pn,'actors':actors,'persistent_after_award':g.mem(md+0xf100,0xa00),'before_audio':before}
def unchanged(q):assert g.mem(md+0xf100,0xa00)==q['persistent_after_award'],q['tag']+' mutated before confirmation'
def open_full(q):
 L.mg_break(s['battle_full_drop_slot']);g.press('A',3,40)
 assert L.mg_hit(),q['tag']+' did not offer full-inventory choice'
 L.mg_break(0xffffffff);g.run(60);unchanged(q)
def open_confirmation(key='A',hold=3):
 if key=='A':
  L.mg_break(s['battle_full_drop_action']);g.press(key,hold,40)
  assert L.mg_hit(),'item action menu was not reached'
  L.mg_break(0xffffffff);g.run(60)
 L.mg_break(s['battle_confirm_drop_discard']);g.press(key,hold,40)
 assert L.mg_hit(),'confirmation was not reached'
 L.mg_break(0xffffffff);g.run(60)
def confirm_yes(q):
 unchanged(q);press('LEFT');press('A');g.run(50)
def finish(q,slot=None,free_slot=None,save=False,used=None):
 expected=bytearray(q['persistent_after_award']);inventory=bytearray(q['inventory'])
 if slot is not None:inventory=inventory[:slot]+inventory[slot+1:]+bytes([q['drop']])
 elif free_slot is not None:inventory[free_slot]=q['drop']
 expected[0x310:0x338]=inventory
 assert g.mem(md+0xf100,0xa00)==expected,q['tag']+' wrong reward/inventory result'
 if used is None:assert g.mem(pp,62*q['pn'])+g.mem(ee,62)==q['actors'],'result UI changed battle actors'
 else:assert g.mem(ee,62)==q['actors'][-62:],'item use changed enemy actor'
 shot(q['tag']+'_FINAL')
 L.mg_break(0x08800000);g.press('A',3,50)
 assert L.mg_hit(),q['tag']+' acknowledgement did not return'
 L.mg_break(0xffffffff)
 assert g.mem(md+0xf100,0xa00)==expected,q['tag']+' acknowledgement repeated rewards'
 if used is not None:assert g.mem(pp,62*q['pn'])+g.mem(ee,62)==q['actors_after_use'],q['tag']+' acknowledgement repeated item use'
 audio={k:v-q['before_audio'][k] for k,v in diag().items()};assert audio['underruns']==0,audio
 roundtrip=[]
 if save:
  roundtrip=save_roundtrip()
 row={'case':q['tag'],'discarded_slot':slot,'free_slot':free_slot,'inventory_and_surrounding_persistent_state_correct':True,'reward_mode':'debug: no rewards' if q['debug'] else 'EXP/Meseta/level awarded exactly once','reward_state_unchanged_during_decisions_and_ack':True,'actors_unchanged_except_item_use':True,'item_use':used,'SRAM_roundtrip_slots':roundtrip,'audio_delta_result_UI_only':audio}
 results.append(row);print(json.dumps(row),flush=True)

inv=bytes(1+i%30 for i in range(40))
q=fixture('FIRST_NO_THEN_YES',inv);shot('PENDING');open_full(q);shot('FIRST_PAGE');open_confirmation();shot('DEFAULT_NO');press('A');unchanged(q)
open_confirmation();confirm_yes(q);finish(q,slot=0,save=True)
q=fixture('LAST_PAGE_SLOT39',inv);open_full(q)
for _ in range(9):press('RIGHT')
for _ in range(3):press('DOWN')
shot('LAST_PAGE');open_confirmation();confirm_yes(q);finish(q,slot=39)
q=fixture('CANCEL_CONFIRMATION',inv);open_full(q)
for _ in range(4):press('RIGHT')
press('DOWN');open_confirmation();press('B');unchanged(q)
open_confirmation('B');confirm_yes(q);finish(q)
q=fixture('LEAVE_NO_THEN_YES',inv);open_full(q);open_confirmation('B');press('A');unchanged(q)
open_confirmation('B');confirm_yes(q);finish(q)
protected=bytes([0x8c])+inv[1:]
q=fixture('PRICELESS_SLOT_BLOCKED',protected);open_full(q)
L.mg_break(s['battle_confirm_drop_discard']);g.press('A',3,60);assert not L.mg_hit(),'priceless inventory item can be discarded'
L.mg_break(0xffffffff);unchanged(q);shot('PROTECTED');press('DOWN');open_confirmation();confirm_yes(q);finish(q,slot=1)
q=fixture('PRICELESS_DROP_KEPT',inv,drop=0x8c);open_full(q)
L.mg_break(s['battle_confirm_drop_discard']);g.press('B',3,60);assert not L.mg_hit(),'priceless new drop can be left behind'
L.mg_break(0xffffffff);unchanged(q);open_confirmation();confirm_yes(q);finish(q,slot=0)
q=fixture('HELD_A_RELEASE_REQUIRED',inv);open_full(q)
L.mg_break(0xffffffff);g.run(120,'A');unchanged(q)
g.run(60);shot('HELD_A_ACTION');press('B');unchanged(q)
open_confirmation('B');confirm_yes(q);finish(q)
for slot in [0,17,39]:
 free=bytearray(inv);free[slot]=0;q=fixture('FREE_SLOT_'+str(slot),bytes(free))
 # This snapshot already contains the automatic drop; use its original bytes
 # as the expected result and verify there is no second item decision.
 assert g.mem(md+0xf410,40)==free[:slot]+bytes([q['drop']])+free[slot+1:]
 q['inventory']=g.mem(md+0xf410,40);finish(q,free_slot=slot)
for tag,rate,debug in [('NO_DROP',0,0),('DEBUG_NO_REWARDS',127,1)]:
 q=fixture(tag,inv,rate=rate,debug=debug);finish(q)

def use_confirmation(q,incoming=False):
 if incoming:press('UP')
 L.mg_break(s['battle_full_drop_action']);g.press('A',3,40)
 assert L.mg_hit(),'use action was not offered'
 L.mg_break(0xffffffff);g.run(60);shot(q['tag']+'_ACTION');unchanged(q)
 press('RIGHT');L.mg_break(s['battle_confirm_drop_discard']);g.press('A',3,40)
 assert L.mg_hit(),'use confirmation was not reached'
 L.mg_break(0xffffffff);g.run(60);shot(q['tag']+'_DEFAULT_NO');unchanged(q)

def use_yes(q,chosen=0,group=False):
 press('LEFT');L.mg_break(s['battle_use_full_drop'] if group else s['battle_choose_ally']);g.press('A',3,60)
 assert L.mg_hit(),'use target/apply was not reached'
 L.mg_break(0xffffffff);g.run(60)
 if not group:
  unchanged(q);shot(q['tag']+'_TARGET')
  for _ in range(chosen):press('RIGHT')
  L.mg_break(s['battle_use_full_drop']);g.press('A',3,60)
  assert L.mg_hit(),'selected use was not applied'
  L.mg_break(0xffffffff);g.run(60)

def check_use(q,slot,checks,save=False):
 before=q['persistent_after_award'];expected=bytearray(before);actors=bytearray(q['actors'])
 for pos,opts in checks.items():
  cid=q['specs'][pos][0];d=0xf500+cid*128;off=d-0xf100;hp=r16(d+14);lo,hi=opts['hp']
  assert lo<=hp<=hi,(q['tag'],'source HP bounds',pos,hp,lo,hi)
  status=opts.get('status',before[off+22]);assert g.rd8(md+d+22)==status,(q['tag'],'status',pos)
  expected[off+14:off+16]=hp.to_bytes(2,'big');expected[off+22]=status
  actors[pos*62:pos*62+2]=hp.to_bytes(2,'little');actors[pos*62+2:pos*62+4]=r16(d+16).to_bytes(2,'little');actors[pos*62+27]=status
  if opts.get('restore'):
   agi=g.rd8(md+d+31);dex=g.rd8(md+d+34);expected[off+32]=agi;expected[off+35]=dex
   for offset,value in [(25,agi),(26,dex),(32,agi),(33,dex)]:actors[pos*62+offset]=value
 inv_expected=q['inventory'] if slot==40 else q['inventory'][:slot]+q['inventory'][slot+1:]+bytes([q['drop']])
 expected[0x310:0x338]=inv_expected
 assert g.mem(md+0xf100,0xa00)==expected,q['tag']+' changed unrelated persistent state'
 assert g.mem(pp,62*q['pn'])+g.mem(ee,62)==actors,q['tag']+' changed unrelated actors'
 shot(q['tag']+'_USED');press('A',after=60)
 # finish independently checks the final inventory and acknowledgement; the
 # allowed HP/status/stat results above become its persistent reference.
 q['persistent_after_award']=bytes(expected)
 q['actors_after_use']=bytes(actors)
 finish(q,slot=None if slot==40 else slot,used={'incoming':slot==40,'targets':list(checks)},save=save)

consumables=bytes([0x7d])+inv[1:]
q=fixture('USE_OLD_CANCEL_TARGET',consumables,specs=[(0,1,0,0)]);open_full(q);use_confirmation(q)
press('A');unchanged(q)  # Default No returns to the inventory.
use_confirmation(q);press('LEFT');L.mg_break(s['battle_choose_ally']);g.press('A',3,60);assert L.mg_hit()
L.mg_break(0xffffffff);g.run(60);press('B');unchanged(q)
use_confirmation(q);use_yes(q);check_use(q,0,{0:{'hp':(min(38,r16(0xf510)),min(59,r16(0xf510)))}})
q=fixture('USE_INCOMING_MONOMATE',inv,drop=0x7d,specs=[(0,1,0,0)]);open_full(q);use_confirmation(q,incoming=True);use_yes(q)
check_use(q,40,{0:{'hp':(min(38,r16(0xf510)),min(59,r16(0xf510)))}},save=True)
q=fixture('USE_SHORTCAKE_ITEM_POWER',bytes([0x91])+inv[1:],specs=[(0,1,0,0)]);open_full(q);use_confirmation(q);use_yes(q)
check_use(q,0,{0:{'hp':(7,16)}})
q=fixture('USE_STAR_DEW_GROUP',bytes([0x83])+inv[1:],specs=[(0,1,0,0),(1,1,0,0),(7,20,0,1)]);open_full(q);use_confirmation(q);use_yes(q,group=True)
check_use(q,0,{0:{'hp':(min(101,r16(0xf510)),min(157,r16(0xf510)))},1:{'hp':(100,100)}})
q=fixture('USE_MOON_DEW_QUARTER',bytes([0x82])+inv[1:],specs=[(0,1,0,0),(1,0,0xfd,0)]);open_full(q);use_confirmation(q);use_yes(q,chosen=1)
check_use(q,0,{1:{'hp':(25,25),'status':0x90,'restore':True}})
q=fixture('USE_MOON_DEW_NO_EFFECT',bytes([0x82])+inv[1:],specs=[(0,1,1,0)]);open_full(q);use_confirmation(q);use_yes(q)
check_use(q,0,{})
q=fixture('USE_REPAIR_KIT_ANDROID',bytes([0x90])+inv[1:],specs=[(0,1,0,0),(6,0,0x43,1)]);open_full(q);use_confirmation(q);use_yes(q,chosen=1)
check_use(q,0,{1:{'hp':(100,100),'status':0x83,'restore':True}})
q=fixture('USE_HUMAN_ITEM_ANDROID_REFUSED',consumables,specs=[(0,1,0,0),(6,20,0,1)]);open_full(q);use_confirmation(q)
press('LEFT');L.mg_break(s['battle_choose_ally']);g.press('A',3,60);assert L.mg_hit();L.mg_break(0xffffffff);g.run(60);press('RIGHT')
L.mg_break(s['battle_use_full_drop']);g.press('A',3,60);assert not L.mg_hit(),'human item accepted an android target'
L.mg_break(0xffffffff);unchanged(q);press('B');unchanged(q);open_confirmation('B');confirm_yes(q);finish(q)
for id in [1,0x84,0x85,0x8b,0x92]:
 q=fixture('USE_DISABLED_'+hex(id),bytes([id])+inv[1:]);open_full(q)
 L.mg_break(s['battle_full_drop_action']);g.press('A',3,40);assert L.mg_hit();L.mg_break(0xffffffff);g.run(60);press('RIGHT')
 L.mg_break(s['battle_confirm_drop_discard']);g.press('A',3,60);assert not L.mg_hit(),'excluded/non-consumable use accepted'
 L.mg_break(0xffffffff);unchanged(q);press('B');unchanged(q);open_confirmation('B');confirm_yes(q);finish(q)

(O/(prefix+'_MGBA_FULL_DROP.json')).write_text(json.dumps({'core':'upstream mGBA 0.10.5','fresh_matching_ROM_boot':not a.reuse,'cases':results,'fixtures':'native result function with actual loaded Chaz/enemy records; controlled level threshold, complete inventory, source-priced items, high drop rate 127/zero; real RNG, button input, IRQs and SRAM APIs','source_discard_and_replace_branch':True,'source_postbattle_USE_branch':True,'normal_save_playthrough_physical_power_cycle_or_audio_quality':False},indent=2)+'\n')
