#!/usr/bin/env python3
"""Source item callbacks and full-drop use transactions under ASan/UBSan.

Independent ROM +1/+3 healing oracle, Moon Dew quarter-HP/$90 status,
Repair Kit bit-6/bit-7 behavior, class/dead filters and all surrounding RAM.
"""
from pathlib import Path
import re,subprocess,json,os
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';s=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;\n]*\)\{',s,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]+'\n'
actor=re.search(r'struct BattleLiteActor\s*\{.*?\};',s,re.S)[0]
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define PS4_INV_BASE 0xf410u
#define PS4_ITEM_DATA 0x2a8e28u
static uint8_t ram[65536],rom[0x300000];static unsigned roll,calls;
static uint8_t ram8r(uint16_t a){return ram[a];}
static uint16_t ram16r(uint16_t a){return (ram[a]<<8)|ram[(uint16_t)(a+1)];}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
static void ram16w(uint16_t a,uint16_t v){ram[a]=v>>8;ram[(uint16_t)(a+1)]=v;}
static uint8_t rom8(uint32_t a){assert(a<sizeof rom);return rom[a];}
static uint16_t rom16(uint32_t a){return (rom8(a)<<8)|rom8(a+1);}
static unsigned ps4_rng2_next(void){calls++;return roll;}
'''+actor+'\n'+''.join(fn(n) for n in ['ps4_item_ptr','ps4_item_price','ps4_item_type','ps4_inventory_add','ps4_inventory_remove','battle_char_record','battle_party_class_valid','battle_source_heal','battle_post_drop_usable','battle_use_full_drop'])+r'''
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 unsigned cases=0,gates=0;uint8_t original[65536],expected[65536];struct BattleLiteActor p[5],want[5];
 const uint8_t usable[]={0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x86,0x90,0x91,0x93};
 for(unsigned id=0;id<152;id++){unsigned yes=0;for(unsigned j=0;j<sizeof usable;j++)yes|=id==usable[j];assert(battle_post_drop_usable(id)==!!yes);gates++;}
 const unsigned slots[]={0,17,39,40};const uint8_t statuses[]={0,1,2,4,0x40,0x7f,0xfd,0x90};const uint8_t ids[]={0,6,7,2,5};
 for(unsigned it=0;it<sizeof usable;it++)for(unsigned sl=0;sl<4;sl++)for(unsigned st=0;st<8;st++)for(unsigned chosen=0;chosen<5;chosen++)for(unsigned rnd=0;rnd<3;rnd++)for(unsigned full=0;full<2;full++){
  uint8_t item=usable[it],drop=slots[sl]==40?item:0x4e;unsigned slot=slots[sl];
  memset(ram,0xa5,sizeof ram);memset(p,0,sizeof p);for(unsigned k=0;k<40;k++)ram[PS4_INV_BASE+k]=1+k%30;if(slot<40)ram[PS4_INV_BASE+slot]=item;
  for(unsigned k=0;k<5;k++){p[k].id=ids[k];p[k].android=k==1||k==2;p[k].status=statuses[(st+k)%8];p[k].max_hp=50;p[k].hp=(p[k].status&0x44)?0:(full?128+k*4:20);p[k].mental=255;p[k].agility=2;p[k].base_agility=1;p[k].dexterity=4;p[k].base_dexterity=3;
   unsigned d=0xf500+ids[k]*128;ram16w(d+14,p[k].hp);ram16w(d+16,128+k*4);ram[d+22]=p[k].status;ram[d+31]=33+k;ram[d+34]=44+k;ram[d+32]=p[k].agility;ram[d+35]=p[k].dexterity;}
  memcpy(original,ram,sizeof ram);memcpy(expected,ram,sizeof ram);memcpy(want,p,sizeof p);roll=rnd==0?0:rnd==1?3:7;calls=0;
  unsigned ip=PS4_ITEM_DATA+(item-1)*22,range=rom[ip+2]&15,effect=rom[ip],multi=range==5||range==7||range==9,changed=0,expect_calls=0;
  int allowed=multi||(range==4?!p[chosen].android:range==6?p[chosen].android:1);
  if(allowed){
   for(unsigned k=0;k<5;k++){
    if(!multi&&k!=chosen)continue;
    if((range==4||range==5)&&want[k].android)continue;if((range==6||range==7)&&!want[k].android)continue;
    unsigned dead=want[k].status&0x44;if(effect==0x15&&!dead)continue;if(effect!=0x15&&effect!=0x16&&dead)continue;
    unsigned d=0xf500+ids[k]*128,mx=128+k*4,newhp=want[k].hp;uint8_t newstatus=want[k].status;want[k].max_hp=mx;
    if(effect==0x12){unsigned sum=8+16*roll;unsigned healing=((sum*rom[ip+1]/64)+rom[ip+1]+2*rom[ip+3])/2;newhp+=healing;if(newhp>mx)newhp=mx;expect_calls+=16;}
    if(item==0x82){newhp=mx/4;newstatus&=0x90;}if(item==0x86){newhp=mx;if(dead)newstatus&=0x90;}if(item==0x90){newhp=mx;newstatus&=~0x40;if(dead&0x40)newstatus|=0x80;}
    if(item==0x80)newstatus&=~1;if(item==0x81)newstatus&=~2;
    changed|=newhp!=want[k].hp||newstatus!=want[k].status;want[k].hp=newhp;want[k].status=newstatus;
    expected[d+14]=newhp>>8;expected[d+15]=newhp;expected[d+22]=newstatus;
    if(effect>=0x13&&effect<=0x16){want[k].agility=want[k].base_agility=33+k;want[k].dexterity=want[k].base_dexterity=44+k;expected[d+32]=33+k;expected[d+35]=44+k;}
   }
   if(slot<40){for(unsigned k=slot;k<39;k++)expected[PS4_INV_BASE+k]=original[PS4_INV_BASE+k+1];expected[PS4_INV_BASE+39]=drop;}
  }
  unsigned result=battle_use_full_drop(drop,slot,p,5,chosen);assert(result==(allowed?(changed?2:1):0));assert(calls==expect_calls);assert(!memcmp(ram,expected,sizeof ram));assert(!memcmp(p,want,sizeof p));cases++;
 }
 // Rejected requests must not touch inventory, party state or RNG.
 for(unsigned item=0;item<152;item++){
  if(battle_post_drop_usable(item))continue;memset(ram,0xa5,sizeof ram);memset(p,0,sizeof p);ram[PS4_INV_BASE]=item;memcpy(original,ram,sizeof ram);memcpy(want,p,sizeof p);calls=0;
  assert(!battle_use_full_drop(0x4e,0,p,5,0));assert(!memcmp(ram,original,sizeof ram)&&!memcmp(p,want,sizeof p)&&!calls);cases++;
 }
 const unsigned badslots[]={41,255,0xffffffffu};
 for(unsigned k=0;k<3;k++){memcpy(original,ram,sizeof ram);assert(!battle_use_full_drop(0x4e,badslots[k],p,5,0));assert(!memcmp(ram,original,sizeof ram));cases++;}
 assert(!battle_use_full_drop(0,0,p,5,0));assert(!battle_use_full_drop(0x4e,0,p,0,0));assert(!battle_use_full_drop(0x4e,0,p,6,0));cases+=3;
 printf("PASS %u full-drop use transactions and %u source eligibility gates: item healing RNG/+1/+3, quarter/full revive, status masks, class/dead filters, source record limits, consume/append, incoming-drop isolation and guarded RAM/actors\n",cases,gates);
}
'''
src=O/'r46_post_drop_use_probe.c';src.write_text(code);exe=O/'r46_post_drop_use_probe'
subprocess.run(['cc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie',str(src),'-o',str(exe)],check=True)
p=subprocess.run([str(exe),str(R/'references/roms/ps4built.bin')],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));assert p.returncode==0,p.stdout+p.stderr
(O/'R46_CP20_POST_DROP_USE_TRANSACTIONS.json').write_text(json.dumps({'result':'PASS','output':p.stdout.strip(),'scope':__doc__,'native_UI_in_this_probe':False,'normal_save_playthrough_verified':False},indent=2)+'\n');print(p.stdout,end='')
