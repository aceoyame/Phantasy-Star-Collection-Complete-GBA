#!/usr/bin/env python3
"""Probe native menu, ATTACK, damage-window and death paths in the rebuilt ROM.

Uses debug state plus explicit actor/enemy fixtures on the supplied custom
non-cycle-accurate/HLE/no-audio core. No ROM test hook is shipped.
"""
from pathlib import Path
import argparse, ast, ctypes, json, re, sys, tempfile
from PIL import Image

R=Path(__file__).resolve().parents[1]; O=R.parent/'build_artifacts'
sys.path.insert(0,str(R/'tools_host/r46_gba_emu'))
from gba import GBA,L,run_until_break
from boot_debug import boot_to_debug
oracle_source=ast.parse((R/'tools/check_r46_gun_reactions.py').read_text())
oracle_node=next(n for n in oracle_source.body if isinstance(n,ast.FunctionDef) and n.name=='oracle')
oracle_globals={'exempt':set((R/'references/roms/ps4built.bin').read_bytes()[0xc0b6:0xc0c2])}
exec(compile(ast.Module(body=[oracle_node],type_ignores=[]),'source_counter_oracle','exec'),oracle_globals)
reaction_oracle=oracle_globals['oracle']
rom=str(R.parent/'binaries/RUN_THIS_RELEASE48_PSC.gba')
s={}
for line in (R/'build-psc-module/ps4_psc_module.map').read_text().splitlines():
 p=line.split()
 if len(p)>=5:
  try:s[p[-1]]=int(p[0],16)
  except ValueError:pass
def mem(g,a,n):return bytes(g.rd8(a+i) for i in range(n))
s['field_pause_pal']=int(re.search(r'field_pause_pal=\(uint16_t\*\)(0x[0-9A-Fa-f]+)',(R/'gba/title_runtime_v06.c').read_text())[1],16)
def put(g,a,b):
 for i,v in enumerate(b):g.wr8(a+i,v)
def call(g,name,args=()):
 for reg,val in enumerate(args[:4]):L.gba_set_reg(reg,val)
 for i,val in enumerate(args[4:]):g.wr32(0x03007d80+i*4,val)
 for reg,val in [(13,0x03007d80),(14,0x08800000),(15,s[name])]:L.gba_set_reg(reg,val)
 assert run_until_break(g,0x08800000,20),name+' did not return'
 return g.reg(0)
ap=argparse.ArgumentParser();ap.add_argument('--reuse-state',action='store_true');ap.add_argument('--prefix',default='CP13');args=ap.parse_args();prefix='R46_'+args.prefix
state=O/(prefix+'_DF3_MENU.bin')
if args.reuse_state:
 g=GBA(rom,entry=0x08800000);g.load(str(state))
else:
 g=boot_to_debug(rom,str(O/(prefix+'_DEBUG.bin')))
 for _ in range(3):g.press('RIGHT',3,20)
 g.press('A',3,300);g.run(120);g.save(str(state))
assert g.rd8(g.rd32(s['battle_ui_enemies'])+20)==0x84
results=[]

# Exercise the actual event-battle command collector, rather than just its
# range helper: all-range guns advance to the next actor without target UI.
for item,expected in ((0x3f,'battle_command_for_actor'),(0x54,'battle_command_for_actor'),(0x3e,'battle_choose_target')):
 g=GBA(rom,entry=0x08800000);g.load(str(state))
 g.press('A',3,20) # CMD -> the first actor's ATTACK/TECH/SKILL menu.
 p=g.rd32(s['battle_ui_party']);e=g.rd32(s['battle_ui_enemies'])
 g.wr8(p+20,7);g.wr8(0x0200f500+7*128+0x4c,item)
 enemy=mem(g,e,62)
 for i in range(1,4):put(g,e+i*62,enemy)
 g.wr32(s['battle_enemy_count'],4)
 L.gba_set_break(ctypes.c_uint32(s[expected]));g.press('A',3,160)
 assert L.gba_break_hit(),'Command collector did not follow '+expected
 L.gba_set_break(ctypes.c_uint32(0xffffffff))
 if item!=0x3e:assert g.reg(0)==p+62,'All-range ATTACK did not advance to next actor'
 results.append({'menu_item':hex(item),'actual_collector_route':expected})
 print('PASS actual command menu',hex(item),expected,flush=True)

cases=[('PULSE_LASER',0x3f,15,255,[0,0,0,0],999),
       ('PULSE_VULCAN',0x54,15,255,[0,0,0,0],999),
       ('PLASMA_RIFLE_SINGLE',0x3e,15,255,[0,0,0,0],999),
       ('PULSE_HOLES',0x3f,10,255,[0,0,0,0],999),
       ('PULSE_MIXED_MISS',0x3f,15,50,[0,255,0,255],999),
       ('PULSE_ALL_MISS',0x3f,15,0,[255,255,255,255],999),
       ('PLASMA_RIFLE_MISS',0x3e,15,0,[255,255,255,255],999),
       ('PHONON_EXTRA',0x79,15,255,[0,0,0,0],999),
       ('BOSS_CRITICAL',0x3e,4,255,[0,0,0,0],999),
       ('EVENT0_PULSE',0x3f,7,255,[0,0,0,0],999),
       ('PULSE_LETHAL',0x54,15,255,[0,0,0,0],1)]
for tag,item,live,dex,agilities,hp in cases:
 g=GBA(rom,entry=0x08800000);g.load(str(state))
 pp,ee,cp,tp=0x0200dd60,0x0200dea0,0x0200dfb0,0x0200dfe0
 # Load original native actor/enemy records, then constrain initiative,
 # hit chance and HP so each source branch has an observable result.
 call(g,'battle_load_char',(6,pp));p=bytearray(mem(g,pp,62))
 p[20]=7;p[0:4]=(999).to_bytes(2,'little')*2;p[8:10]=(300).to_bytes(2,'little')
 p[25]=255;p[26]=dex;p[27]=0;p[28]=0
 put(g,pp,p+bytes(248));put(g,0x0200f500+7*128+0x4c,bytes([item,0]))
 for i in range(4):
  eid=0x78 if tag=='BOSS_CRITICAL' and i==2 else 12 if tag=='EVENT0_PULSE' and i==1 else 9
  call(g,'battle_load_enemy',(eid,ee+i*62));g.wr16(ee+i*62,hp if live&(1<<i) else 0)
  g.wr16(ee+i*62+2,999);g.wr8(ee+i*62+25,agilities[i]);g.wr8(ee+i*62+27,0)
  for j in range(14):g.wr8(ee+i*62+34+j,4)
  g.wr8(s['battle_enemy_placement']+i,((16,23,30,38) if tag=='EVENT0_PULSE' else (8,18,28,38))[i])
 before=mem(g,ee,248)
 g.wr32(s['battle_ui_party'],pp);g.wr32(s['battle_ui_enemies'],ee)
 g.wr32(s['battle_ui_party_count'],1);g.wr32(s['battle_enemy_count'],3 if tag=='EVENT0_PULSE' else 4)
 g.wr8(s['battle_event_index'],0 if tag=='EVENT0_PULSE' else 1)
 call(g,'battle_anim_reset');call(g,'battle_ui_bind',(pp,1,ee,2))
 g.wr8(s['battle_palette_valid'],0);g.wr8(s['battle_palette_hw_dirty'],1)
 g.wr8(s['battle_enemy_death_pending_mask'],0);g.wr8(s['battle_enemy_death_visible_mask'],0)
 put(g,s['battle_enemy_reaction_flags'],bytes(4))
 # Populate the canonical scene before timing/capturing the action. Debug
 # fixture palette invalidation is setup, not a frame of the weapon effect.
 call(g,'battle_compose_igg',(pp,1,ee,2,0));call(g,'battle_present_page0')
 palette_before=mem(g,s['field_pause_pal'],512)
 # All-range commands ignore this selected enemy; single-range hits slot 2.
 put(g,cp,bytes([1,0,2,255])+bytes(16));g.wr32(tp,2)
 inv=mem(g,0x0200f410,40);uses=mem(g,0x0200f500+7*128+0x6a,16)
 g.wr32(0x03007d80,0xffffffff);g.wr32(0x03007d84,tp)
 for reg,val in [(0,pp),(1,1),(2,ee),(3,cp),(13,0x03007d80),(14,0x08800000),(15,s['battle_resolve_mixed_round'])]:L.gba_set_reg(reg,val)
 # Hit/damage RNG executes once before the gun controller, with HP restored
 # for presentation. Break both real production phases to verify that order.
 assert run_until_break(g,s['battle_execute_cmd'],80)
 execute_lr=g.reg(14);assert run_until_break(g,execute_lr&~1,20)
 after=[g.rd16(ee+i*62) for i in range(4)]
 expected_mask=4 if item in (0x3e,0x79) else live
 if tag=='PULSE_MIXED_MISS':expected_mask=5
 if tag in ('PULSE_ALL_MISS','PLASMA_RIFLE_MISS'):expected_mask=0
 changed=sum((after[i]!=int.from_bytes(before[i*62:i*62+2],'little'))<<i for i in range(4))
 assert changed==expected_mask,(tag,after,expected_mask)
 flags=g.rd8(s['battle_slasher_hit_mask']);assert flags&15==expected_mask
 if item in (0x3e,0x79) and expected_mask:assert flags>>4==4,'Guaranteed critical not retained'
 elif item not in (0x3e,0x79):assert flags>>4==0,'Multi-target attack retained a critical'
 # The compiler inlines the gun controller into this production dispatch.
 assert run_until_break(g,s['battle_party_source_attack'],80),'Weapon visual dispatch not selected'
 visual_lr=g.reg(14);f0=g.frame()
 visual_state=tempfile.NamedTemporaryFile(suffix='.bin')
 g.save(visual_state.name)
 assert mem(g,ee,248)==before,'Precomputed damage was not restored before graphics'
 # Inspect actual production tracks, not a reimplemented graphic dispatcher.
 assert run_until_break(g,s['battle_r46_weapon_draw'],80)
 tracks,n=g.reg(0),g.reg(1);native_tracks=[]
 for i in range(n):native_tracks.append([g.rd16(tracks+i*8),g.rd16(tracks+i*8+2)])
 launch_count=4 if item in (0x3f,0x54) else 2 if item==0x79 else 1
 impacts=native_tracks[launch_count:]
 eligible=[i for i in range(4) if live&(1<<i)] if item in (0x3f,0x54) else [2]
 starts=[13+8*j for j in range(len(eligible))] if expected_mask else []
 if item==0x3e and expected_mask:starts+=[21]
 assert [t[1] for t in impacts]==starts,(tag,native_tracks,starts)
 reaction_samples=[];frames=[]
 if tag in ('PLASMA_RIFLE_SINGLE','PULSE_MIXED_MISS','BOSS_CRITICAL','EVENT0_PULSE'):
  ids=[g.rd8(ee+i*62+20) for i in range(4)]
  sequence=reaction_oracle(ids,live,flags,item in (0x3f,0x54),2)
  end=47
  for index,start in native_tracks:
   a=s['r46_weapon_anims']+index*12;holds=s['r46_weapon_bank']+g.rd32(a+4)
   end=max(end,start+sum(g.rd8(holds+j) for j in range(g.rd8(a+3))))
  for ordinal,i in enumerate(eligible):
   if flags>>i&1 and ids[i] not in oracle_globals['exempt']:end=max(end,17+ordinal*8+15)
  for source_tick in range(1,end):
   if source_tick>1:
    assert run_until_break(g,s['battle_r46_gun_reaction_tick'],40)
    assert g.reg(3)==source_tick
    assert run_until_break(g,s['battle_r46_weapon_draw'],40)
   assert g.reg(2)==source_tick
   flash,white=sequence[source_tick*2:source_tick*2+2]
   assert g.rd8(s['battle_physical_flash_mask'])==flash,(tag,source_tick,flash)
   for j in range(13):
    original=int.from_bytes(palette_before[(193+j)*2:(194+j)*2],'little')
    assert g.rd16(0x05000000+(193+j)*2)==(0x7fff if white else original),(tag,source_tick,j,'palette not published')
   if tag=='EVENT0_PULSE':
    # Production hw_hide_obj moves the sprite to Y=160, rather than setting
    # the generic fixture's OBJ-disable bit. Check the real native convention.
    for i in range(3):assert (g.rd16(0x07000000+(0,1,2)[i]*8)==160)==bool(flash&(1<<i)),(tag,source_tick,i,'idle OBJ visibility')
   reaction_samples.append([source_tick,flash,white])
   if source_tick in (10,17,20,21,25,28,33,41,44,50):frames.append(g.shot())
 L.gba_set_break(ctypes.c_uint32(visual_lr&~1))
 for tick in range(400):
  g.run(1)
  if tick and tick%4==0:frames.append(g.shot())
  if L.gba_break_hit():break
 assert L.gba_break_hit(),'Gun visual did not return'
 visual_frames=g.frame()-f0
 assert mem(g,ee,248)==before and mem(g,pp,62)==p,'Visual changed HP/actors'
 assert not g.rd8(s['battle_physical_flash_mask']) and mem(g,s['field_pause_pal'],512)==palette_before,'Gun palette/flash state not restored'
 assert run_until_break(g,s['battle_finish_damage'],20),'Gun visual did not finish'
 # Production finish is reached with the saved gameplay HP committed once.
 assert [g.rd16(ee+i*62) for i in range(4)]==after
 assert mem(g,pp,62)==p,'Visual changed actor'
 assert mem(g,s['battle_enemy_reaction_flags'],4)==bytes(1 if expected_mask&(1<<i) else 0 for i in range(4))
 assert mem(g,pp,62)==p and mem(g,0x0200f410,40)==inv and mem(g,0x0200f500+7*128+0x6a,16)==uses
 finish_lr=g.reg(14);damage_image=None;L.gba_set_break(ctypes.c_uint32(finish_lr&~1))
 for tick in range(300):
  g.run(1)
  if g.rd8(s['battle_damage_enemy_mask'])==expected_mask and expected_mask and g.rd8(s['battle_damage_columns'])==5:
   damage_image=g.shot()
  if L.gba_break_hit():break
 assert L.gba_break_hit(),'Native damage/death step did not return'
 assert [g.rd16(ee+i*62) for i in range(4)]==after,'Damage window reapplied or lost damage'
 assert g.rd8(s['battle_suppress_skill_source_audio'])==0
 if hp==1:
  assert after==[0]*4 and g.rd8(s['battle_enemy_death_pending_mask'])==15
  assert run_until_break(g,s['battle_process_pending_enemy_deaths'],20)
  death_lr=g.reg(14);assert run_until_break(g,death_lr&~1,300)
  assert not g.rd8(s['battle_enemy_death_pending_mask']) and not g.rd8(s['battle_enemy_death_visible_mask'])
 # Repeated mid-scanline breakpoints restart this custom core's scanline loop
 # and invalidate its partially rendered framebuffer. Replay presentation
 # independently with only the return breakpoint for coherent image evidence.
 if reaction_samples:
  g=GBA(rom,entry=0x08800000);g.load(visual_state.name)
  L.gba_set_break(ctypes.c_uint32(visual_lr&~1));frames=[];replay_start=g.frame()
  for tick in range(400):
   g.run(1)
   if tick and tick%4==0:frames.append(g.shot())
   if L.gba_break_hit():break
  assert L.gba_break_hit(),'Coherent visual replay did not return'
  assert mem(g,ee,248)==before and mem(g,pp,62)==p
  visual_frames=g.frame()-replay_start
 visual_state.close()
 out=Image.new('RGB',(720,320))
 for i in range(6):out.paste(frames[min(len(frames)-1,i*(len(frames)-1)//5)],((i%3)*240,(i//3)*160))
 out.save(O/(prefix+'_'+tag+'.png'))
 if expected_mask:
  assert damage_image is not None,'No full native damage window was observed'
  damage_image.resize((480,320),Image.Resampling.NEAREST).save(O/(prefix+'_'+tag+'_DAMAGE.png'))
 results.append({'case':tag,'item':hex(item),'visual_display_frames':visual_frames,
                 'damaged_mask':changed,'hp_after':after,'visual_preserves_actors':True,
                 'resources_unchanged':True,'native_damage_window_preserves_result':True,
                 'physical_reaction_bits':True,'all_death_objects_complete':hp==1})
 results[-1].update({'physical_hit_flags':flags,'native_weapon_tracks':native_tracks,'impact_starts':starts,
                     'gameplay_before_visual_once':True,'hp_restored_for_visual':True,
                     'source_tick_reaction_palette_samples':reaction_samples,'palette_flash_restored':True})
 print('PASS native',tag,'mask',hex(changed),'HP',after,flush=True)
(O/(prefix+'_PHYSICAL_RANGE_EMULATOR_EVIDENCE.json')).write_text(json.dumps({
 'core':'Supplied custom GBA; non-cycle-accurate/HLE/no audio output',
 'fixtures':'Fresh-ROM debug battle; native event command collector plus mixed-round actor/enemy fixtures. Actual graphics, gameplay, damage windows and all four death objects executed.',
 'screenshots':'Separate coherent replay for per-tick traced cases; custom-core mid-frame breakpoint artifacts excluded. Frame counts are not hardware timing proof.',
 'cases':results,'full_normal_save_or_hardware_certified':False},indent=2)+'\n')
