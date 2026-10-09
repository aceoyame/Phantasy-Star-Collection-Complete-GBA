#!/usr/bin/env python3
"""Independent source oracles for Medic Power and Ataraxia gameplay.

ASan/UBSan runs extracted production helpers. Field expectations independently
step UpdateRNGSeed and reject repeated rolls; battle expectations use original
technique/skill stat selectors, base powers and controlled RNG2 words. Entire
RAM and actor structures are compared.
"""
from pathlib import Path
import json, os, re, subprocess

S = Path(__file__).resolve().parents[1]
O = S.parent / 'build_artifacts'
source = (S / 'gba/title_runtime_v06.c').read_text()

def function(name):
    m = re.search(r'^static[^\n]*\b' + name + r'\([^;\n]*\)\{', source, re.M)
    assert m, name
    p, depth = m.end(), 1
    while depth:
        depth += (source[p] == '{') - (source[p] == '}')
        p += 1
    return source[m.start():p] + '\n'

actor = re.search(r'struct BattleLiteActor\s*\{.*?\};', source, re.S)[0]
command = re.search(r'struct BattleCommandLite\s*\{.*?\};', source, re.S)[0]
code = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define PS4_INV_BASE 0xf410u
#define PS4_ITEM_DATA 0x2a8e28u
#define PS4_TECH_DATA 0x2a9be8u
#define PS4_SKILL_DATA 0x2a9d28u
static uint8_t ram[65536],rom[0x300000];static unsigned roll,calls;
static uint8_t ram8r(uint16_t a){return ram[a];}
static uint16_t ram16r(uint16_t a){return (ram[a]<<8)|ram[(uint16_t)(a+1)];}
static uint32_t ram32r(uint16_t a){return ((uint32_t)ram16r(a)<<16)|ram16r(a+2);}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
static void ram16w(uint16_t a,uint16_t v){ram[a]=v>>8;ram[(uint16_t)(a+1)]=v;}
static void ram32w(uint16_t a,uint32_t v){ram16w(a,v>>16);ram16w(a+2,v);}
static uint8_t rom8(uint32_t a){assert(a<sizeof rom);return rom[a];}
static uint16_t rom16(uint32_t a){return (rom8(a)<<8)|rom8(a+1);}
static unsigned ps4_rng2_next(void){calls++;return roll;}
''' + actor + '\n' + command + '\n' + ''.join(function(n) for n in [
    'field_npc_rng_next', 'ps4_party_ids', 'battle_char_record',
    'battle_party_class_valid', 'battle_actor_stat', 'battle_source_heal',
    'ps4_field_ability_ptr', 'ps4_field_healing_ability',
    'ps4_field_ability_resource', 'ps4_field_ability_healing',
    'ps4_field_use_ability', 'battle_apply_recovery_skill'
]) + r'''
static unsigned get16(const uint8_t*b,unsigned a){return b[a]*256+b[a+1];}
static void put16(uint8_t*b,unsigned a,unsigned v){b[a]=v>>8;b[a+1]=v;}
static void put32(uint8_t*b,unsigned a,uint32_t v){put16(b,a,v>>16);put16(b,a+2,v);}
static unsigned field_oracle(uint8_t*b,unsigned ip,unsigned stat){
 unsigned e=rom[ip];if(e==0x16)return 999;if(e!=0x12&&e!=0x17)return 0;
 uint32_t seed=((uint32_t)get16(b,0xef0c)<<16)|get16(b,0xef0e);
 unsigned accepted=0,sum=0,last=0;
 while(accepted<16){
  if(!(seed&65535))seed=0x2a6d365b;
  uint32_t product=seed*41u;unsigned low=product&65535,high=product>>16;
  unsigned value=(low+high)&65535;seed=(value<<16)|low;unsigned q=value%8;
  if(q!=last){sum+=q;last=q;accepted++;}
 }
 put32(b,0xef0c,seed);
 return (sum+8)*stat/128+stat/2+rom[ip+3];
}

static void setup(struct BattleLiteActor*p,unsigned pn,unsigned st,unsigned value,unsigned full,unsigned rnd,unsigned id){
 const unsigned ids[]={6,0,1,7,8};const uint8_t statuses[]={0,1,2,4,8,0x20,0x40,0x90,0xfd,0xff};
 memset(ram,0xa5,sizeof ram);memset(p,0,sizeof(*p)*5);calls=0;roll=(const unsigned[]){0,3,7}[rnd];
 for(unsigned i=0;i<5;i++){
  ram[0xf40a+i]=i<pn?ids[i]:0xff;p[i].id=ids[i];p[i].android=ids[i]==6||ids[i]==7;
  p[i].status=i?statuses[(st+i-1)%10]:0;p[i].max_hp=600+i*20;p[i].max_tp=128+i*4;
  p[i].hp=p[i].status&0x44?0:(full?p[i].max_hp:1);p[i].tp=full?p[i].max_tp:2;
  p[i].strength=value;p[i].mental=255-value;p[i].agility=2;p[i].dexterity=4;p[i].base_agility=3;p[i].base_dexterity=5;
  unsigned d=0xf500+ids[i]*128;ram16w(d+14,p[i].hp);ram16w(d+16,p[i].max_hp);ram16w(d+18,p[i].tp);ram16w(d+20,p[i].max_tp);ram[d+22]=p[i].status;
  ram[d+25]=value;ram[d+28]=255-value;ram[d+31]=33+i;ram[d+34]=44+i;ram[d+32]=2;ram[d+35]=4;
  memset(ram+d+0x62,0,8);memset(ram+d+0x6a,0,16);ram[d+0x62+3]=id;ram[d+0x70]=1;
 }
 ram32w(0xef0c,(const uint32_t[]){0,0x12345678,0xffffffff}[rnd]);
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 const unsigned values[]={0,1,31,255};uint8_t expected[65536];struct BattleLiteActor p[5],want[5];unsigned bc=0,fc=0,gates=0;
 for(unsigned skill=0;skill<2;skill++)for(unsigned id=0;id<=55;id++){
  unsigned yes=(!skill&&((id>=0x18&&id<=0x1d)||(id>=0x22&&id<=0x25)))||(skill&&(id==0x2a||id==0x2b||id==0x2c||id==0x2d||id==0x36));
  assert(ps4_field_healing_ability(skill,id)==!!yes);gates++;
 }
 for(unsigned id=0x2d;id<=0x2e;id++)for(unsigned pn=1;pn<=5;pn++)for(unsigned st=0;st<10;st++)for(unsigned v=0;v<4;v++)for(unsigned full=0;full<2;full++)for(unsigned rnd=0;rnd<3;rnd++){
  unsigned ip=PS4_SKILL_DATA+(id-1)*8;setup(p,pn,st,values[v],full,rnd,id);memcpy(want,p,sizeof p);memcpy(expected,ram,sizeof ram);
  unsigned stat=rom[ip+1]==1?p[0].strength:p[0].mental,expect_calls=0;
  for(unsigned k=0;k<pn;k++){
   if(!want[k].android&&(id==0x2d||!(want[k].status&0x44))){
    unsigned amount=((8+16*roll)*stat/64+stat+2*rom[ip+3])/2;if(amount>999)amount=999;expect_calls+=16;
    if(id==0x2d){unsigned hp=want[k].hp+amount;want[k].hp=hp>want[k].max_hp?want[k].max_hp:hp;want[k].status&=0x90;}
    else{unsigned tp=want[k].tp+amount;want[k].tp=tp>want[k].max_tp?want[k].max_tp:tp;}
   }
   if(id==0x2e)want[k].status&=~8;
  }
  battle_apply_recovery_skill(p,pn,0,id,battle_actor_stat(&p[0],rom[ip+1]),rom[ip+3]);
  assert(calls==expect_calls&&!memcmp(p,want,sizeof p)&&!memcmp(ram,expected,sizeof ram));bc++;
  setup(p,pn,st,values[v],full,rnd,id);memcpy(expected,ram,sizeof ram);unsigned changed=0;
  if(id==0x2e){assert(!ps4_field_use_ability(1,6,id,11)&&!memcmp(ram,expected,sizeof ram)&&!calls);gates++;continue;}
  unsigned cd=0xf500+6*128;expected[cd+0x70]--;
  for(unsigned k=0;k<(pn<5?pn+1:5);k++){
   unsigned statoff=(const unsigned[]){0,25,28,31,34}[rom[ip+1]&15];unsigned add=field_oracle(expected,ip,expected[cd+statoff]);if(k==pn)break;
   unsigned target=expected[0xf40a+k];if(target==6||target==7)continue;
   unsigned d=0xf500+target*128,hp=get16(expected,d+14),mx=get16(expected,d+16),next=hp+add;if(next>mx)next=mx;
   changed|=next!=hp||expected[d+22]!=0;put16(expected,d+14,next);expected[d+22]=0;
  }
  assert(ps4_field_use_ability(1,6,id,11)==(changed?2u:1u));assert(!memcmp(ram,expected,sizeof ram)&&!calls);fc++;
 }
 for(unsigned st=0;st<256;st++){
  setup(p,5,0,31,0,1,0x2d);unsigned cd=0xf500+6*128;ram[cd+22]=st;memcpy(expected,ram,sizeof ram);
  unsigned used=ps4_field_use_ability(1,6,0x2d,11);assert(!!used==!(st&6));
  if(st&6)assert(!memcmp(ram,expected,sizeof ram));else assert(ram[cd+0x70]==0);gates++;
 }
 // Resources and invalid requests cannot spend or step RNG. No minimum HP is
 // invented when Strength is zero: Medic Power still applies its source mask.
 setup(p,5,0,0,0,1,0x2d);memcpy(expected,ram,sizeof ram);memcpy(want,p,sizeof p);
 battle_apply_recovery_skill(p,0,0,0x2d,31,0);battle_apply_recovery_skill(p,6,0,0x2d,31,0);battle_apply_recovery_skill(p,5,5,0x2d,31,0);battle_apply_recovery_skill(p,5,0,0x2c,31,0);
 assert(!memcmp(p,want,sizeof p)&&!memcmp(ram,expected,sizeof ram)&&!calls);gates+=4;
 unsigned cd=0xf500+6*128;
 assert(!ps4_field_use_ability(1,11,0x2d,11)&&!ps4_field_use_ability(1,10,0x2d,11));assert(!memcmp(ram,expected,sizeof ram));gates+=2;
 ram[cd+0x70]=0;memcpy(expected,ram,sizeof ram);assert(!ps4_field_use_ability(1,6,0x2d,11)&&!memcmp(ram,expected,sizeof ram));gates++;
 ram[cd+0x70]=1;ram[cd+0x65]=0;memcpy(expected,ram,sizeof ram);assert(!ps4_field_use_ability(1,6,0x2d,11)&&!memcmp(ram,expected,sizeof ram));gates++;
 printf("PASS %u special recovery battle transactions, %u field Medic Power transactions and %u eligibility/resource gates; independent ROM stat/power/RNG, revival/status/TP/class/sentinel rules and complete RAM/actor checks\n",bc,fc,gates);
}
'''
src = O / 'r46_special_recovery_rules_probe.c'
exe = O / 'r46_special_recovery_rules_probe'
src.write_text(code)
subprocess.run(['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
                '-fno-omit-frame-pointer', '-no-pie', str(src), '-o', str(exe)], check=True)
p = subprocess.run([str(exe), str(S / 'references/roms/ps4built.bin')],
                   capture_output=True, text=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
assert p.returncode == 0, p.stdout + p.stderr
(O / 'R46_CP23_SPECIAL_RECOVERY_RULES.json').write_text(json.dumps({
    'result': 'PASS', 'output': p.stdout.strip(), 'scope': __doc__,
    'native_UI_in_this_probe': False, 'physical_or_normal_save_playthrough_verified': False
}, indent=2) + '\n')
print(p.stdout, end='')
