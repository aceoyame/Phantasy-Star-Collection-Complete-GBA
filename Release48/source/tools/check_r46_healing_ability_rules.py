#!/usr/bin/env python3
"""Independent original-ROM oracles for battle and field healing abilities.

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
    'ps4_field_use_ability', 'battle_apply_healing_ability'
]) + r'''
static unsigned get16(const uint8_t*b,unsigned a){return b[a]*256+b[a+1];}
static void put16(uint8_t*b,unsigned a,unsigned v){b[a]=v>>8;b[a+1]=v;}
static void put32(uint8_t*b,unsigned a,uint32_t v){put16(b,a,v>>16);put16(b,a+2,v);}
static unsigned field_oracle(uint8_t*b,unsigned ip,unsigned stat){
 unsigned e=rom[ip];if(e==0x16)return 999;if(e!=0x12)return 0;
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
static void setup(struct BattleLiteActor*p,unsigned item,unsigned slot,unsigned st,unsigned full,unsigned seed,unsigned pn){
 const unsigned ids[]={0,6,7,1,5};const uint8_t statuses[]={0,1,2,4,0x40,0x7f,0xfd,0x90};
 memset(ram,0xa5,sizeof ram);memset(p,0,sizeof(*p)*5);
 for(unsigned i=0;i<40;i++)ram[0xf410+i]=1+i%30;ram[0xf410+slot]=item;
 for(unsigned i=0;i<5;i++){
  ram[0xf40a+i]=i<pn?ids[i]:0xff;
  p[i].id=ids[i];p[i].android=i==1||i==2;p[i].status=statuses[(st+i)%8];
  p[i].max_hp=128+i*4;p[i].hp=(p[i].status&0x44)?0:(full?p[i].max_hp:20);
  p[i].mental=255;p[i].agility=2;p[i].base_agility=1;p[i].dexterity=4;p[i].base_dexterity=3;
  if(i==4){p[i].status=0;p[i].hp=20;p[i].mental=st&1?0:255;}
  unsigned d=0xf500+ids[i]*128;ram16w(d+14,p[i].hp);ram16w(d+16,p[i].max_hp);
  ram[d+22]=p[i].status;ram[d+31]=33+i;ram[d+34]=44+i;ram[d+32]=2;ram[d+35]=4;
 }
 const uint32_t seeds[]={0,0x12345678,0xffffffff};ram32w(0xef0c,seeds[seed]);calls=0;roll=seed==0?0:seed==1?3:7;
}

int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 const unsigned ids[]={0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x22,0x23,0x24,0x25,0x2a,0x2b,0x2c,0x36};
 uint8_t expected[65536];struct BattleLiteActor p[5],want[5];unsigned bc=0,fc=0,gates=0;
 for(unsigned skill=0;skill<2;skill++)for(unsigned id=0;id<=55;id++){
  unsigned yes=skill&&id==0x2d;for(unsigned j=0;j<14;j++)yes|=(j>=10)==skill&&ids[j]==id;
  assert(ps4_field_healing_ability(skill,id)==!!yes);gates++;
 }
 for(unsigned it=0;it<14;it++)for(unsigned st=0;st<8;st++)for(unsigned chosen=0;chosen<5;chosen++)for(unsigned rnd=0;rnd<3;rnd++)for(unsigned full=0;full<2;full++){
  unsigned skill=it>=10,id=ids[it],ip=(skill?PS4_SKILL_DATA:PS4_TECH_DATA)+(id-1)*8,e=rom[ip],r=rom[ip+2]&15,multi=r==5||r==7||r==9;
  setup(p,1,0,st,full,rnd,5);unsigned cd=0xf500+5*128;ram[cd+22]=0;
  p[4].strength=st&1?0:255;p[4].mental=st&1?255:0;
  unsigned stat=skill?(rom[ip+1]==1?p[4].strength:p[4].mental):p[4].mental;
  ram[cd+25]=p[4].strength;ram[cd+28]=p[4].mental;
  memset(ram+cd+0x52,0,16);memset(ram+cd+0x62,0,8);ram[cd+(skill?0x62:0x52)+3]=id;ram[cd+0x70]=1;ram16w(cd+18,100);
  memcpy(want,p,sizeof p);memcpy(expected,ram,sizeof ram);unsigned expect_calls=0;
  for(unsigned k=0;k<5;k++){
   if(!multi&&k!=(r==3?4:chosen))continue;if((r==4||r==5)&&want[k].android)continue;if((r==6||r==7)&&!want[k].android)continue;
   unsigned dead=want[k].status&0x44;if(e==0x15&&!dead)continue;if(e!=0x15&&e!=0x16&&dead)continue;
   if(e==0x12){unsigned amount=((8+16*roll)*stat/64+stat+2*rom[ip+3])/2;unsigned hp=want[k].hp+amount;want[k].hp=hp>want[k].max_hp?want[k].max_hp:hp;expect_calls+=16;}
   if(e==0x13)want[k].status&=~1;if(e==0x14)want[k].status&=~2;
   if(e==0x15||e==0x16){want[k].hp=e==0x15?want[k].max_hp/4:want[k].max_hp;want[k].status&=0x90;}
   if(e>=0x13&&e<=0x16){unsigned d=0xf500+want[k].id*128;want[k].agility=want[k].base_agility=33+k;want[k].dexterity=want[k].base_dexterity=44+k;expected[d+32]=33+k;expected[d+35]=44+k;}
  }
  battle_apply_healing_ability(p,5,4,chosen,e,stat,rom[ip+3],r);
  assert(calls==expect_calls&&!memcmp(ram,expected,sizeof ram)&&!memcmp(p,want,sizeof p));bc++;
  memcpy(ram,expected,sizeof ram); // Restore field fixture independently of actor mutations.
  for(unsigned k=0;k<5;k++){unsigned d=0xf500+p[k].id*128;ram16w(d+14,(p[k].status&0x44)?0:(full?p[k].max_hp:20));ram[d+22]=k==4?0:(const uint8_t[]){0,1,2,4,0x40,0x7f,0xfd,0x90}[(st+k)%8];ram[d+32]=2;ram[d+35]=4;}
  memcpy(expected,ram,sizeof ram);unsigned changed=0;calls=0;
  if(skill)expected[cd+0x70]--;else put16(expected,cd+18,100-rom[ip+1]);
  for(unsigned k=0;k<(multi?5u:1u);k++){
   unsigned add=field_oracle(expected,ip,stat),target=multi?expected[0xf40a+k]:(r==3?5:p[chosen].id),d=0xf500+target*128;
   unsigned android=target==6||target==7,status=expected[d+22];
   if((!skill&&android)||(skill&&(id==0x2a||id==0x36)&&!android))continue;
   if(e==0x15){if(!(status&4))continue;}else if(e!=0x16&&(status&4))continue;
   unsigned hp=get16(expected,d+14),mx=get16(expected,d+16);if(e==0x15)add=mx/4;
   unsigned next=hp+add;if(next>mx)next=mx;unsigned mask=e==0x12?15:e==0x13?14:e==0x14?13:0;
   changed|=next!=hp||(status&mask)!=status;put16(expected,d+14,next);expected[d+22]&=mask;
  }
  assert(ps4_field_use_ability(skill,5,id,p[chosen].id)==(changed?2u:1u));assert(!calls&&!memcmp(ram,expected,sizeof ram));fc++;
 }
 // Exact field caster gates differ from battle: sealed bit 4 is allowed,
 // dead/paralyzed bits block. A confirmed ineffective use still spends once.
 for(unsigned it=0;it<14;it++)for(unsigned st=0;st<256;st++){
  unsigned skill=it>=10,id=ids[it],ip=(skill?PS4_SKILL_DATA:PS4_TECH_DATA)+(id-1)*8;
  setup(p,1,0,0,0,1,5);unsigned cd=0xf500+5*128;ram[cd+22]=st;memset(ram+cd+0x52,0,16);memset(ram+cd+0x62,0,8);ram[cd+(skill?0x62:0x52)]=id;ram[cd+0x6a]=1;ram16w(cd+18,rom[ip+1]);
  memcpy(expected,ram,sizeof ram);unsigned used=ps4_field_use_ability(skill,5,id,0);assert(!!used==!(st&6));if(st&6)assert(!memcmp(ram,expected,sizeof ram));else assert(skill?ram[cd+0x6a]==0:ram16r(cd+18)==0);gates++;
 }
 // Group sentinel rolls occur even with fewer than five party slots.
 for(unsigned skill=0;skill<2;skill++)for(unsigned pn=1;pn<=3;pn+=2){
  unsigned id=skill?0x2c:0x1b,ip=(skill?PS4_SKILL_DATA:PS4_TECH_DATA)+(id-1)*8;
  setup(p,1,0,0,0,1,pn);unsigned cd=0xf500;ram[cd+22]=0;ram[cd+28]=31;memset(ram+cd+0x52,0,16);memset(ram+cd+0x62,0,8);ram[cd+(skill?0x62:0x52)]=id;ram[cd+0x6a]=1;ram16w(cd+18,100);
  memcpy(expected,ram,sizeof ram);if(skill)expected[cd+0x6a]--;else put16(expected,cd+18,100-rom[ip+1]);
  for(unsigned k=0;k<=pn;k++){unsigned add=field_oracle(expected,ip,31);if(k==pn)break;unsigned target=expected[0xf40a+k],d=0xf500+target*128;if((!skill&&(target==6||target==7))||(expected[d+22]&4))continue;unsigned hp=get16(expected,d+14)+add,mx=get16(expected,d+16);put16(expected,d+14,hp>mx?mx:hp);expected[d+22]&=15;}
  assert(ps4_field_use_ability(skill,0,id,11));assert(!memcmp(ram,expected,sizeof ram));fc++;
 }
 // Missing/depleted knowledge/resources and invalid single targets are inert.
 for(unsigned it=0;it<14;it++){
  unsigned skill=it>=10,id=ids[it];setup(p,1,0,0,0,1,5);unsigned cd=0xf500+5*128;ram[cd+22]=0;memset(ram+cd+0x52,0,16);memset(ram+cd+0x62,0,8);ram16w(cd+18,0);ram[cd+0x6a]=0;
  memcpy(expected,ram,sizeof ram);assert(!ps4_field_use_ability(skill,5,id,0)&&!memcmp(ram,expected,sizeof ram));gates++;
  ram[cd+(skill?0x62:0x52)]=id;memcpy(expected,ram,sizeof ram);assert(!ps4_field_use_ability(skill,5,id,0)&&!memcmp(ram,expected,sizeof ram));gates++;
 }
 // Invalid casters and invalid single targets must not spend or roll RNG.
 for(unsigned it=0;it<14;it++){
  unsigned skill=it>=10,id=ids[it],ip=(skill?PS4_SKILL_DATA:PS4_TECH_DATA)+(id-1)*8,r=rom[ip+2]&15;
  setup(p,1,0,0,0,1,5);unsigned cd=0xf500+5*128;ram[cd+22]=0;memset(ram+cd+0x52,0,16);memset(ram+cd+0x62,0,8);ram[cd+(skill?0x62:0x52)]=id;ram[cd+0x6a]=1;ram16w(cd+18,100);
  memcpy(expected,ram,sizeof ram);assert(!ps4_field_use_ability(skill,11,id,0)&&!ps4_field_use_ability(skill,10,id,0));assert(!memcmp(ram,expected,sizeof ram));gates+=2;
  if(r==4||r==8){assert(!ps4_field_use_ability(skill,5,id,11)&&!ps4_field_use_ability(skill,5,id,10));assert(!memcmp(ram,expected,sizeof ram));gates+=2;}
 }
 printf("PASS %u battle healing transactions, %u field ability transactions and %u resource/eligibility gates; original-ROM stats, RNG, status/class/self/group/revival rules, complete RAM/actor checks\n",bc,fc,gates);
}
'''
src = O / 'r46_healing_ability_rules_probe.c'
exe = O / 'r46_healing_ability_rules_probe'
src.write_text(code)
subprocess.run(['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
                '-fno-omit-frame-pointer', '-no-pie', str(src), '-o', str(exe)], check=True)
p = subprocess.run([str(exe), str(S / 'references/roms/ps4built.bin')],
                   capture_output=True, text=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
assert p.returncode == 0, p.stdout + p.stderr
(O / 'R46_CP22_HEALING_ABILITY_RULES.json').write_text(json.dumps({
    'result': 'PASS', 'output': p.stdout.strip(), 'scope': __doc__,
    'native_UI_in_this_probe': False, 'physical_or_normal_save_playthrough_verified': False
}, indent=2) + '\n')
print(p.stdout, end='')
