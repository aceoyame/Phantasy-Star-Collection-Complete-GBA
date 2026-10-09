#!/usr/bin/env python3
"""Guarded original-ROM/source oracle for the nine party support abilities."""
from pathlib import Path
import json, os, re, subprocess
S=Path(__file__).resolve().parents[1]
O=S.parent/'build_artifacts'
source=(S/'gba/title_runtime_v06.c').read_text()
rom=(S/'references/roms/ps4built.bin').read_bytes()
records=[(0,id_) for id_ in range(0x1e,0x22)]+[(1,id_) for id_ in [0x26,0x27,0x28,0x29,0x35]]
found=[(skill,id_) for skill,base,count in [(0,0x2a9be8,38),(1,0x2a9d28,54)] for id_ in range(1,count+1) if 9<=rom[base+(id_-1)*8]<=13]
assert found==records
# Original machine code writes power+$2E without doubling power. Feeve $0B
# therefore addresses the WATER restore byte at $39, not live PSY at $44.
assert rom[0x6382:0x6394].hex()=='10300003024000ff0640002e19bc00010000'
def function(name):
    m=re.search(r'^static[^\n]*\b'+name+r'\([^;\n]*\)\{',source,re.M)
    assert m,name
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
#define PS4_TECH_DATA 0x2a9be8u
#define PS4_SKILL_DATA 0x2a9d28u
static uint8_t ram[65536],rom[0x300000],want_ram[65536];
static uint8_t ram8r(uint16_t a){return ram[a];}
static uint16_t ram16r(uint16_t a){return ram[a]*256+ram[(uint16_t)(a+1)];}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
static void ram16w(uint16_t a,uint16_t v){ram[a]=v>>8;ram[(uint16_t)(a+1)]=v;}
static uint8_t rom8(uint32_t a){assert(a<sizeof rom);return rom[a];}
''' + actor+'\n'+''.join(function(n) for n in [
    'battle_char_record','battle_party_class_valid','battle_actor_stat',
    'ps4_field_ability_ptr','ps4_field_healing_ability','battle_apply_support_ability'
])+r'''
static unsigned get16(const uint8_t*b,unsigned a){return b[a]*256+b[a+1];}
static void put16(uint8_t*b,unsigned a,unsigned v){b[a]=v>>8;b[a+1]=v;}
static void oracle(struct BattleLiteActor*w,unsigned pn,unsigned actor,unsigned chosen,unsigned ip,unsigned stat){
 unsigned effect=rom[ip],range=rom[ip+2]&15;
 for(unsigned k=0;k<pn;k++){
  if((range==3&&k!=actor)||((range==4||range==6||range==8)&&k!=(chosen<pn?chosen:actor)))continue;
  if(w[k].status&0x44)continue;
  if((range==4||range==5)&&w[k].android)continue;
  if((range==6||range==7)&&!w[k].android)continue;
  unsigned d=0xf500+w[k].id*128,offset=0,value=0;
  switch(effect){
   case 9:offset=0x26;value=(get16(want_ram,d+0x24)+stat)&65535;w[k].atk=value;break;
   case 10:offset=0x2a;value=(get16(want_ram,d+0x28)+stat)&65535;w[k].def=value;break;
   case 11:offset=0x2e;value=(get16(want_ram,d+0x2c)+stat)&65535;w[k].magic_def=value;break;
   case 12:offset=0x20;value=(want_ram[d+0x1f]+stat)&255;w[k].agility=value;break;
   case 13:assert(rom[ip+3]==11);offset=0x39;value=1;w[k].base_element_prop[4]=1;break;
   default:assert(0);
  }
  if(offset==0x20||offset==0x39)want_ram[d+offset]=value;
  else put16(want_ram,d+offset,value);
 }
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 const unsigned ids[]={0,6,1,7,8},values[]={0,1,37,255},statuses[]={0,1,2,4,8,0x10,0x40,0xff};
 const unsigned skill[]={0,0,0,0,1,1,1,1,1},ability[]={0x1e,0x1f,0x20,0x21,0x26,0x27,0x28,0x29,0x35};
 struct BattleLiteActor p[5],want[5];unsigned cases=0,gates=0;
 for(unsigned record=0;record<9;record++){
  unsigned ip=(skill[record]?PS4_SKILL_DATA:PS4_TECH_DATA)+(ability[record]-1)*8;
  assert(!(rom[ip+2]&0x20));assert(!ps4_field_healing_ability(skill[record],ability[record]));gates++;
  for(unsigned pn=1;pn<=5;pn++)for(unsigned act=0;act<pn;act++)for(unsigned chosen=0;chosen<=pn;chosen++)
  for(unsigned st=0;st<8;st++)for(unsigned vi=0;vi<4;vi++)for(unsigned wrap=0;wrap<2;wrap++){
   memset(ram,0xa5,sizeof ram);memset(p,0,sizeof p);
   for(unsigned k=0;k<5;k++){
    unsigned d=0xf500+ids[k]*128;
    p[k].id=ids[k];p[k].android=ids[k]==6||ids[k]==7;p[k].status=statuses[(st+k)%8];
    p[k].hp=p[k].status&0x44?0:123;p[k].max_hp=999;p[k].tp=60;p[k].max_tp=128;
    p[k].strength=values[vi];p[k].mental=255-values[vi];p[k].agility=2;p[k].dexterity=7;
    p[k].base_agility=wrap?250:17+k;p[k].base_atk=wrap?65530:111+k;p[k].base_def=wrap?65531:71+k;p[k].base_magic_def=wrap?65532:61+k;
    p[k].atk=13;p[k].def=5;p[k].magic_def=11;
    ram[d+0x1f]=p[k].base_agility;ram[d+0x20]=p[k].agility;
    ram16w(d+0x24,p[k].base_atk);ram16w(d+0x26,p[k].atk);
    ram16w(d+0x28,p[k].base_def);ram16w(d+0x2a,p[k].def);
    ram16w(d+0x2c,p[k].base_magic_def);ram16w(d+0x2e,p[k].magic_def);
    for(unsigned e=0;e<14;e++){p[k].element_prop[e]=2+e;p[k].base_element_prop[e]=3+e;ram[d+0x30+e*2]=2+e;ram[d+0x31+e*2]=3+e;}
   }
   memcpy(want,p,sizeof p);memcpy(want_ram,ram,sizeof ram);
   // Independent source record selection: techniques use Mental; skills 1/2
   // use Strength/Mental, with deliberately different values.
   unsigned selector=skill[record]?rom[ip+1]&127:2;
   assert(selector==1||selector==2);unsigned stat=selector==1?p[act].strength:p[act].mental;
   oracle(want,pn,act,chosen,ip,stat);
   battle_apply_support_ability(p,pn,act,chosen,rom[ip],battle_actor_stat(&p[act],selector),rom[ip+3],rom[ip+2]&15);
   assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));
   // Applying it again must replace from the base, preserving whole RAM and
   // every unrelated actor byte even after prior buffs/debuffs.
   battle_apply_support_ability(p,pn,act,chosen,rom[ip],stat,rom[ip+3],rom[ip+2]&15);
   assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));cases++;
  }
 }
 for(unsigned n=0;n<7;n++)for(unsigned act=0;act<7;act++)if(!n||n>5||act>=n){
  memset(p,0x5a,sizeof p);memcpy(want,p,sizeof p);memcpy(want_ram,ram,sizeof ram);
  battle_apply_support_ability(p,n,act,0,9,255,0,9);
  assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));gates++;
 }
 printf("{\"support_transactions\":%u,\"repeat_nonstacking_checks\":%u,\"eligibility_bounds_gates\":%u,\"full_RAM_actor_guards\":true,\"ASan_UBSan\":true}\n",cases,cases,gates);
}
'''
probe=O/'r46_cp24_support_probe.c';exe=O/'r46_cp24_support_probe'
probe.write_text(code)
subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(probe),'-o',str(exe)],check=True)
r=subprocess.run([str(exe),str(S/'references/roms/ps4built.bin')],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
assert r.returncode==0,r.stdout+r.stderr
result=json.loads(r.stdout)
result['abilities']=[{'kind':'SKILL' if skill else 'TECH','id':f'{id_:02X}','record_hex':rom[(0x2a9d28 if skill else 0x2a9be8)+(id_-1)*8:(0x2a9d28 if skill else 0x2a9be8)+id_*8].hex()} for skill,id_ in records]
result['Feeve_ROM_6382_6393']=rom[0x6382:0x6394].hex()
(O/'R46_CP24_SUPPORT_RULES.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
