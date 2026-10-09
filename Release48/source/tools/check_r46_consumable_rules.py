#!/usr/bin/env python3
"""Independent original-ROM oracles for battle and field consumable transactions.

ASan/UBSan runs extracted production helpers. Field expectations independently
step UpdateRNGSeed and reject repeated rolls; battle expectations use ROM item
+1/+3 and controlled RNG2 words. Entire RAM and actor structures are compared.
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
    'field_npc_rng_next', 'ps4_item_ptr', 'ps4_item_effect', 'ps4_item_type',
    'ps4_party_ids', 'ps4_inventory_remove', 'battle_char_record',
    'battle_party_class_valid', 'battle_source_heal', 'ps4_field_item_usable',
    'ps4_field_item_healing', 'ps4_field_use_item', 'battle_apply_item_friendly',
    'battle_healing_item_slot', 'battle_use_healing_item'
]) + r'''
static unsigned get16(const uint8_t*b,unsigned a){return b[a]*256+b[a+1];}
static void put16(uint8_t*b,unsigned a,unsigned v){b[a]=v>>8;b[a+1]=v;}
static void put32(uint8_t*b,unsigned a,uint32_t v){put16(b,a,v>>16);put16(b,a+2,v);}
static unsigned field_oracle(uint8_t*b,unsigned ip){
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
 return (sum+8)*rom[ip+1]/128+rom[ip+1]/2+rom[ip+3];
}
static void remove_oracle(uint8_t*b,unsigned slot){
 for(unsigned i=slot;i<39;i++)b[0xf410+i]=b[0xf411+i];b[0xf437]=0;
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
 const uint8_t items[]={0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x86,0x90,0x91,0x93};
 const unsigned slots[]={0,17,39,17};unsigned bc=0,fc=0,gates=0;
 uint8_t expected[65536],original[65536];struct BattleLiteActor p[5],want[5];
 for(unsigned id=0;id<152;id++){unsigned yes=0;for(unsigned j=0;j<sizeof items;j++)yes|=id==items[j];assert(ps4_field_item_usable(id)==!!yes);gates++;}
 for(unsigned it=0;it<sizeof items;it++)for(unsigned sl=0;sl<4;sl++)for(unsigned st=0;st<8;st++)for(unsigned chosen=0;chosen<5;chosen++)for(unsigned rnd=0;rnd<3;rnd++)for(unsigned full=0;full<2;full++){
  unsigned item=items[it],slot=slots[sl],ip=PS4_ITEM_DATA+(item-1)*22,e=rom[ip],r=rom[ip+2]&15,multi=r==5||r==7||r==9;
  setup(p,item,slot,st,full,rnd,5);memcpy(expected,ram,sizeof ram);memcpy(want,p,sizeof p);unsigned expect_calls=0;
  remove_oracle(expected,slot);
  for(unsigned k=0;k<5;k++){
   if(!multi&&k!=chosen)continue;if((r==4||r==5)&&want[k].android)continue;if((r==6||r==7)&&!want[k].android)continue;
   unsigned dead=want[k].status&0x44;if(e==0x15&&!dead)continue;if(e!=0x15&&e!=0x16&&dead)continue;
   if(e==0x12){unsigned amount=((8+16*roll)*rom[ip+1]/64+rom[ip+1]+2*rom[ip+3])/2;unsigned hp=want[k].hp+amount;want[k].hp=hp>want[k].max_hp?want[k].max_hp:hp;expect_calls+=16;}
   if(item==0x82){want[k].hp=want[k].max_hp/4;want[k].status&=0x90;}
   if(item==0x86){want[k].hp=want[k].max_hp;if(dead)want[k].status&=0x90;}
   if(item==0x90){want[k].hp=want[k].max_hp;want[k].status&=~0x40;if(dead&0x40)want[k].status|=0x80;}
   if(item==0x80)want[k].status&=~1;if(item==0x81)want[k].status&=~2;
   if(e>=0x13&&e<=0x16){unsigned d=0xf500+want[k].id*128;want[k].agility=want[k].base_agility=33+k;want[k].dexterity=want[k].base_dexterity=44+k;expected[d+32]=33+k;expected[d+35]=44+k;}
  }
  struct BattleCommandLite cmd={4,item,chosen,sl==3?0xff:slot};assert(battle_use_healing_item(p,5,4,&cmd)==1);
  assert(calls==expect_calls&&!memcmp(ram,expected,sizeof ram)&&!memcmp(p,want,sizeof p));bc++;
  setup(p,item,slot,st,full,rnd,5);memcpy(expected,ram,sizeof ram);unsigned changed=0;
  for(unsigned k=0;k<(multi?5u:1u);k++){
   unsigned add=field_oracle(expected,ip),id=multi?expected[0xf40a+k]:p[chosen].id,d=0xf500+id*128;
   unsigned android=id==6||id==7,status=expected[d+22];
   if(multi){if(android||(status&4))continue;}
   else if(item==0x90){if(!android)continue;}
   else{if(android)continue;if(item==0x82){if(!(status&4))continue;}else if(item!=0x86&&(status&4))continue;}
   unsigned oldhp=get16(expected,d+14),mx=get16(expected,d+16);if(item==0x82)add=mx/4;
   unsigned hp=oldhp+add;if(hp>mx)hp=mx;unsigned mask=e==0x12?15:e==0x13?14:e==0x14?13:0,newstatus=status&mask;
   changed|=hp!=oldhp||status!=newstatus;put16(expected,d+14,hp);expected[d+22]=newstatus;
  }
  remove_oracle(expected,slot);assert(ps4_field_use_item(slot,p[chosen].id)==(changed?2u:1u));
  assert(!memcmp(ram,expected,sizeof ram)&&!calls);fc++;
 }
 // Partial Star Dew parties consume the sentinel's pre-check healing rolls.
 for(unsigned pn=1;pn<=3;pn+=2)for(unsigned rnd=0;rnd<3;rnd++){
  setup(p,0x83,0,0,0,rnd,pn);memcpy(expected,ram,sizeof ram);unsigned ip=PS4_ITEM_DATA+0x82*22;
  for(unsigned k=0;k<=pn;k++){unsigned add=field_oracle(expected,ip);if(k==pn)break;unsigned id=expected[0xf40a+k],d=0xf500+id*128;if(id==6||id==7||(expected[d+22]&4))continue;unsigned hp=get16(expected,d+14)+add,mx=get16(expected,d+16);put16(expected,d+14,hp>mx?mx:hp);expected[d+22]&=15;}
  remove_oracle(expected,0);assert(ps4_field_use_item(0,0)==2&&!memcmp(ram,expected,sizeof ram));fc++;
 }
 // Stale manual index re-finds the item after an earlier turn compacts it.
 setup(p,0x91,17,0,0,0,5);ps4_inventory_remove(0);struct BattleCommandLite cmd={4,0x91,0,17};assert(battle_healing_item_slot(&cmd)==16);assert(battle_use_healing_item(p,5,4,&cmd));bc++;
 memcpy(expected,ram,sizeof ram);memcpy(want,p,sizeof p);unsigned oldcalls=calls;
 assert(battle_healing_item_slot(&cmd)==-1&&!battle_use_healing_item(p,5,4,&cmd));assert(calls==oldcalls&&!memcmp(ram,expected,sizeof ram)&&!memcmp(p,want,sizeof p));bc++;
 for(unsigned id=0;id<152;id++){
  if(ps4_field_item_usable(id))continue;setup(p,id,0,0,0,0,5);memcpy(expected,ram,sizeof ram);memcpy(want,p,sizeof p);cmd=(struct BattleCommandLite){4,id,0,0};
  assert(!ps4_field_use_item(0,0)&&!battle_use_healing_item(p,5,4,&cmd));assert(!calls&&!memcmp(ram,expected,sizeof ram)&&!memcmp(p,want,sizeof p));bc++;fc++;
 }
 setup(p,0x7d,0,0,0,0,5);memcpy(expected,ram,sizeof ram);memcpy(want,p,sizeof p);cmd=(struct BattleCommandLite){4,0x7d,0,0};
 assert(!ps4_field_use_item(40,0)&&!ps4_field_use_item(0,11)&&!ps4_field_use_item(0,10));fc+=3;
 assert(!battle_use_healing_item(p,0,0,&cmd)&&!battle_use_healing_item(p,6,0,&cmd)&&!battle_use_healing_item(p,5,5,&cmd));bc+=3;
 assert(!calls&&!memcmp(ram,expected,sizeof ram)&&!memcmp(p,want,sizeof p));
 printf("PASS %u battle transactions, %u field transactions and %u eligibility gates: independent item power/RNG streams, status/revival/class/group rules, source consumption, stale/missing resources and full RAM/actor guards\n",bc,fc,gates);
}
'''
src = O / 'r46_consumable_rules_probe.c'
exe = O / 'r46_consumable_rules_probe'
src.write_text(code)
subprocess.run(['cc', '-std=c11', '-O1', '-g', '-fsanitize=address,undefined',
                '-fno-omit-frame-pointer', '-no-pie', str(src), '-o', str(exe)], check=True)
p = subprocess.run([str(exe), str(S / 'references/roms/ps4built.bin')],
                   capture_output=True, text=True, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
assert p.returncode == 0, p.stdout + p.stderr
(O / 'R46_CP21_CONSUMABLE_RULES.json').write_text(json.dumps({
    'result': 'PASS', 'output': p.stdout.strip(), 'scope': __doc__,
    'native_UI_in_this_probe': False, 'physical_or_normal_save_playthrough_verified': False
}, indent=2) + '\n')
print(p.stdout, end='')
