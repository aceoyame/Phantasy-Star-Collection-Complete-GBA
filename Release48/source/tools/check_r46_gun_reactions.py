#!/usr/bin/env python3
"""Production gun reaction C versus a source-order object-counter oracle.

No generated art/audio changes. Hardware composition is a fixture; actual
native composition and palette publication are checked by the emulator probe.
"""
from pathlib import Path
import json, re, struct, subprocess, os
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
s=(R/'gba/title_runtime_v06.c').read_text()
rom=(R/'references/roms/ps4built.bin').read_bytes()
asm=(R/'references/disassembly/ps4disasm/ps4disasm/ps4.asm').read_text(encoding='latin1')
assert asm.index('jsr\t(Battle_UpdatePaletteObjs).l',asm.index('GameMode_Battle:')) < asm.index('jsr\tBattle_UpdateFighters(pc)',asm.index('GameMode_Battle:'))
assert 'moveq\t#1, d3\n\tmoveq\t#2, d5' in asm
exempt=set(rom[0xc0b6:0xc0c2]);assert exempt=={0x78,0x79,0x7f,0x82,0x83,0x84,0x85,0x86,0x87,0x8b,0x8c,0x98}
def fn(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;\n]*\)\{',s,re.M);assert m,name
 p=m.end();depth=1
 while depth:depth+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]+'\n'

def oracle(ids,live,flags,all_targets,target):
 """Run original phase counters in palette -> fighter -> object order."""
 fighters=[None]*4;children=[];palette=None;root_phase=0;root_timer=8
 output=[];affected=[i for i in range(4) if live>>i&1 and (all_targets or i==target)]
 for tick in range(65):
  white=0
  if palette is not None:
   palette-=1
   if palette>0:white=bool(palette&2)
   else:palette=None
  flash=0
  for i in range(4):
   if fighters[i] is None:continue
   if ids[i] in exempt or not flags>>i&1:fighters[i]=None;continue
   if fighters[i]==15 and flags>>(i+4)&1:palette=14
   fighters[i]-=1
   if fighters[i]>0:
    if fighters[i]&2:flash|=1<<i
   else:fighters[i]=None
  root_timer-=1
  if root_timer<0:
   if root_phase==0:root_phase=1;root_timer=4
   elif root_phase==1:
    if not flags&15:root_phase=3;root_timer=30
    else:root_phase=2;root_timer=2
   elif root_phase==2:
    children=[[i,n*8] for n,i in enumerate(affected)];root_phase=3;root_timer=30
  next_children=[]
  for i,timer in children:
   timer-=1
   if timer<0:fighters[i]=15
   else:next_children.append([i,timer])
  children=next_children
  output.extend((flash,int(white)))
 return bytes(output)

fixtures=[]
def add(ids,live,flags,all_targets,target,groups,event):
 fixtures.append(bytes(ids)+bytes([live,flags,all_targets,target,groups,event])+oracle(ids,live,flags,all_targets,target))
for eid in range(153):
 for flags in (0,1,17):
  for groups in (0,15):
   for event in (0,1):add([eid]*4,15,flags,0,0,groups,event)
 for flags in range(16):add([eid]*4,15,flags,1,2,5,1)
for live in range(16):
 for flags in range(16):
  if flags&~live:continue
  for event in (0,1):add([9,0x78,0x46,0x84],live,flags,1,2,10,event)
f=O/'r46_gun_reaction_fixtures.bin';f.write_bytes(b''.join(fixtures))
pre=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define BATTLE_MAX_ENEMIES 4
struct BattleLiteActor {uint8_t id;uint16_t hp;};
static uint8_t rom[0x300000],battle_slasher_hit_mask,battle_physical_flash_mask,battle_palette_hw_dirty,battle_enemy_placement[4];
static unsigned battle_enemy_count=4,battle_generic_pending_mask,battle_event_index,syncs;
static uint16_t field_pause_pal[256];
static uint8_t rom8(unsigned p){assert(p<sizeof rom);return rom[p];}
static uint16_t md2gba(unsigned c){assert(c==0xeee);return 0x7fff;}
static void battle_obj_sync(const struct BattleLiteActor*e,unsigned n){(void)e;assert(n==0);syncs++;}
'''
main=r'''
int main(int argc,char**argv){
 assert(argc==3);FILE*r=fopen(argv[1],"rb");assert(r&&fread(rom,1,sizeof rom,r)==sizeof rom);fclose(r);
 FILE*f=fopen(argv[2],"rb");assert(f);unsigned cases=0;uint8_t q[140];
 while(fread(q,1,sizeof q,f)==sizeof q){
  struct BattleLiteActor e[4];for(unsigned i=0;i<4;i++){e[i].id=q[i];e[i].hp=q[4]>>i&1?999:0;battle_enemy_placement[i]=q[8]>>i&1?128:0;}
  uint16_t orig[13];for(unsigned i=0;i<256;i++)field_pause_pal[i]=i*19;
  for(unsigned j=0;j<13;j++)orig[j]=field_pause_pal[193+j];
  for(unsigned j=0;j<16;j++)field_pause_pal[224+j]=(j==0||j==2)?0:0x7fff;
  battle_slasher_hit_mask=q[5];battle_event_index=q[9];battle_physical_flash_mask=0;syncs=0;
  for(unsigned t=0;t<65;t++){
   unsigned old=battle_physical_flash_mask,oldsync=syncs;battle_generic_pending_mask=0;battle_palette_hw_dirty=0;
   battle_r46_gun_reaction_tick(e,q[6],q[7],t,orig);
   unsigned flash=q[10+t*2],white=q[11+t*2];assert(battle_physical_flash_mask==flash);
   assert(battle_generic_pending_mask==(old^flash));assert(syncs-oldsync==(!q[9]&&(old!=flash)));
   for(unsigned j=0;j<13;j++)assert(field_pause_pal[193+j]==(white?0x7fff:orig[j]));
   assert(field_pause_pal[226]==((flash&~q[8])&&white?0x7fff:0));
   for(unsigned j=0;j<193;j++)assert(field_pause_pal[j]==j*19);
   for(unsigned j=206;j<224;j++)assert(field_pause_pal[j]==j*19);
   for(unsigned j=240;j<256;j++)assert(field_pause_pal[j]==j*19);
  }
  assert(!battle_physical_flash_mask);for(unsigned j=0;j<13;j++)assert(field_pause_pal[193+j]==orig[j]);cases++;
 }
 fclose(f);printf("PASS %u source-counter gun reaction cases / %u ticks: 153 IDs, boss exemptions, hit/miss/critical, holes, both palette groups, event-0/generic dirty masks, exact restoration\n",cases,cases*65);return 0;
}
'''
c=O/'r46_gun_reaction_probe.c';c.write_text(pre+fn('battle_r46_hit_exempt')+fn('battle_r46_gun_reaction_tick')+main)
exe=O/'r46_gun_reaction_probe'
subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
p=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin'),str(f)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
(O/'R46_GUN_REACTION_CHECK.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
# Exact nibble transform equivalence for every byte, with both palette states.
for byte in range(256):
 for white in (False,True):
  source=[0 if n==0 else (2 if n==2 else 1) for n in (byte>>4,byte&15)]
  source_colors=[0 if n==0 else (0 if n==2 and not white else 0x7fff) for n in source]
  lowered=[0 if n==0 or (n==2 and not white) else 0x7fff for n in (byte>>4,byte&15)]
  assert source_colors==lowered
(O/'R46_GUN_REACTION_EVIDENCE.json').write_text(json.dumps({'result':'PASS','cases':len(fixtures),'source_ticks':len(fixtures)*65,'nibble_transform_cases':512,'scope':'Actual production reaction C versus independent palette/fighter/object counter simulation and original ROM exemption bytes, ASan/UBSan. Hardware/actor/staging/audio are fixtures. Native composition checked separately; not device timing or a full battle.'},indent=2)+'\n')
print(p.stdout,end='')
