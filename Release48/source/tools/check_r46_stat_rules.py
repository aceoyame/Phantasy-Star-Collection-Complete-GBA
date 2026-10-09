#!/usr/bin/env python3
"""Guarded original-source Vision and stat-debuff rules; no production stubs."""
from pathlib import Path
import json,os,re,subprocess
S=Path(__file__).resolve().parents[1];O=S.parent/'build_artifacts'
source=(S/'gba/title_runtime_v06.c').read_text()
rom=(S/'references/roms/ps4built.bin').read_bytes()
assert rom[0x2a9d28+46*8:0x2a9d28+47*8].hex()=='2600150000000000'
records=[(0,0x14),(0,0x15),(1,0x1c),(1,0x1d),(1,0x1e)]
assert [(skill,id_) for skill,base,count in [(0,0x2a9be8,38),(1,0x2a9d28,54)] for id_ in range(1,count+1) if rom[base+(id_-1)*8] in (3,4,6,0x21)]==records
assert [id_ for id_ in range(1,55) if rom[0x2a9d28+(id_-1)*8+1]&127==0]==[0x2f]
def function(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;\n]*\)\{',source,re.M);assert m,name
 p,depth=m.end(),1
 while depth:
  depth+=(source[p]=='{')-(source[p]=='}');p+=1
 return source[m.start():p]+'\n'
actor=re.search(r'struct BattleLiteActor\s*\{.*?\};',source,re.S)[0]
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static uint8_t ram[65536],want_ram[65536];
static uint8_t ram8r(uint16_t a){return ram[a];}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
''' + actor+r'''
static void battle_enemy_set_hp(struct BattleLiteActor*a,unsigned hp){assert(!"non-stat effect in stat probe");}
''' + ''.join(function(n) for n in ['battle_char_record','battle_party_class_valid','battle_apply_vision_skill','battle_apply_enemy_effect'])+r'''
int main(void){
 const unsigned ids[]={0,6,9,7,8},names[]={0,2,10,17,254,255},dex[]={0,1,31,250,255};
 const unsigned statuses[]={0,1,2,4,8,0x10,0x40,0xff};
 struct BattleLiteActor p[5],want[5];unsigned vision=0,down=0,gates=0;
 for(unsigned pn=1;pn<=5;pn++)for(unsigned actor=0;actor<pn;actor++)
 for(unsigned st=0;st<8;st++)for(unsigned n=0;n<6;n++)for(unsigned b=0;b<5;b++){
  memset(ram,0xa5,sizeof ram);memset(p,0,sizeof p);
  for(unsigned i=0;i<5;i++){
   unsigned d=0xf500+ids[i]*128;p[i].id=ids[i];p[i].android=ids[i]==6||ids[i]==7;
   p[i].status=statuses[(st+i)%8];p[i].hp=0;p[i].max_hp=100;p[i].tp=20;
   p[i].mental=255-names[n];p[i].dexterity=7;p[i].base_dexterity=dex[b];ram[d+0x22]=dex[b];ram[d+0x23]=7;
   ram[d]=names[n];
  }
  memcpy(want,p,sizeof p);memcpy(want_ram,ram,sizeof ram);
  unsigned first_name=want_ram[0xf500+ids[actor]*128];
  for(unsigned i=0;i<pn;i++)if(!want[i].android&&!(want[i].status&0x44)){
   unsigned d=0xf500+ids[i]*128;
   want[i].dexterity=(want_ram[d+0x22]+first_name)&255;want_ram[d+0x23]=want[i].dexterity;
  }
  battle_apply_vision_skill(p,pn,actor,ram8r(0xf500+ids[actor]*128));
  assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));
  battle_apply_vision_skill(p,pn,actor,first_name);
  assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));vision++;
 }
 const unsigned effects[]={3,4,6,0x21},base[]={0,1,31,255,256,32768,65535},stats[]={0,1,31,255,256,32768,65535};
 for(unsigned e=0;e<4;e++)for(unsigned b=0;b<7;b++)for(unsigned n=0;n<7;n++){
  memset(p,0x5a,sizeof p);memcpy(want,p,sizeof p);memcpy(want_ram,ram,sizeof ram);
  p[0].base_atk=want[0].base_atk=base[b];p[0].base_def=want[0].base_def=base[b];
  p[0].base_agility=want[0].base_agility=base[b]&255;p[0].base_dexterity=want[0].base_dexterity=base[b]&255;
  unsigned mask=e<2?65535:255,bv=base[b]&mask,sv=stats[n]&mask;
  unsigned value=bv>sv?bv-sv:(e<2?0:1);
  if(e==0)want[0].atk=value;else if(e==1)want[0].def=value;else if(e==2)want[0].agility=value;else want[0].dexterity=value;
  battle_apply_enemy_effect(p,effects[e],stats[n]);
  assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));
  battle_apply_enemy_effect(p,effects[e],stats[n]);
  assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));down++;
 }
 for(unsigned n=0;n<7;n++)for(unsigned act=0;act<7;act++)if(!n||n>5||act>=n){
  memcpy(want,p,sizeof p);memcpy(want_ram,ram,sizeof ram);battle_apply_vision_skill(p,n,act,255);
  assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));gates++;
 }
 printf("{\"Vision_transactions\":%u,\"stat_down_width_floor_transactions\":%u,\"repeat_checks\":%u,\"bounds_gates\":%u,\"whole_RAM_all_actor_guards\":true,\"ASan_UBSan\":true}\n",vision,down,vision+down,gates);
}
'''
probe=O/'r46_cp25_stat_probe.c';exe=O/'r46_cp25_stat_probe';probe.write_text(code)
subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(probe),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
assert r.returncode==0,r.stdout+r.stderr
report=json.loads(r.stdout)
report['debuff_records']=[{'kind':'SKILL' if skill else 'TECH','id':f'{id_:02X}','record_hex':rom[(0x2a9d28 if skill else 0x2a9be8)+(id_-1)*8:(0x2a9d28 if skill else 0x2a9be8)+id_*8].hex()} for skill,id_ in records]
report['original_Vision_record']='2600150000000000'
report['Vision_original_selector_zero']='first encoded name byte at caster stats offset zero'
(O/'R46_CP25_STAT_RULES.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

