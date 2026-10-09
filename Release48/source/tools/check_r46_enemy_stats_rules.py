#!/usr/bin/env python3
"""Independent guarded source buff/restore width oracles for CP26."""
from pathlib import Path
import json,os,re,subprocess
S=Path(__file__).resolve().parents[1];O=S.parent/'build_artifacts'
source=(S/'gba/title_runtime_v06.c').read_text()
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
#define BATTLE_MAX_ENEMIES 4
static unsigned battle_enemy_count;
static uint8_t ram[65536],want_ram[65536];
static uint8_t ram8r(uint16_t a){return ram[a];}
static uint16_t ram16r(uint16_t a){return ram[a]*256+ram[(uint16_t)(a+1)];}
static void ram8w(uint16_t a,uint8_t v){ram[a]=v;}
static void ram16w(uint16_t a,uint16_t v){ram[a]=v>>8;ram[(uint16_t)(a+1)]=v;}
static void put16(uint8_t*b,unsigned a,unsigned v){b[a]=v>>8;b[a+1]=v;}
''' + actor+'\n'+''.join(function(n) for n in ['battle_char_record','battle_apply_enemy_support','battle_restore_party_stats'])+r'''
int main(void){
 const unsigned values[]={0,1,20,255,256,65515,65535};
 const unsigned effects[]={9,10,11,12,0x2b},statuses[]={0,1,2,4,8,0x10,0x40,0xff};
 struct BattleLiteActor e[4],want_e[4],p[5],want_p[5];unsigned buffs=0,restores=0;
 for(unsigned n=1;n<=4;n++)for(unsigned caster=0;caster<n;caster++)
 for(unsigned f=0;f<5;f++)for(unsigned s=0;s<7;s++)for(unsigned all=0;all<2;all++)
 for(unsigned st=0;st<8;st++){
  memset(e,0x5a,sizeof e);memset(ram,0xa5,sizeof ram);
  for(unsigned i=0;i<4;i++){
   e[i].hp=0;e[i].status=statuses[(st+i)%8];e[i].base_agility=250;
   e[i].base_atk=65530;e[i].base_def=65531;e[i].base_magic_def=65532;
   e[i].strength=251;e[i].mental=252;e[i].agility=253;e[i].dexterity=254;
   e[i].atk=65533;e[i].def=65534;e[i].magic_def=65535;
  }
  memcpy(want_e,e,sizeof e);memcpy(want_ram,ram,sizeof ram);battle_enemy_count=n;
  for(unsigned repeat=0;repeat<2;repeat++){
   for(unsigned i=0;i<n;i++){
    if((!all&&i!=caster)||(want_e[i].status&0x44))continue;
    switch(effects[f]){
     case 9:want_e[i].atk=(want_e[i].base_atk+values[s])&65535;break;
     case 10:want_e[i].def=(want_e[i].base_def+values[s])&65535;break;
     case 11:want_e[i].magic_def=(want_e[i].base_magic_def+values[s])&65535;break;
     case 12:want_e[i].agility=(want_e[i].base_agility+values[s])&255;break;
     case 0x2b:
      want_e[i].strength=(want_e[i].strength+20)&255;want_e[i].mental=(want_e[i].mental+20)&255;
      want_e[i].agility=(want_e[i].agility+20)&255;want_e[i].dexterity=(want_e[i].dexterity+20)&255;
      want_e[i].atk=(want_e[i].atk+20)&65535;want_e[i].def=(want_e[i].def+20)&65535;want_e[i].magic_def=(want_e[i].magic_def+20)&65535;break;
    }
   }
   battle_apply_enemy_support(e,caster,effects[f],values[s],all?2:3);
   assert(!memcmp(e,want_e,sizeof e)&&!memcmp(ram,want_ram,sizeof ram));
  }
  buffs++;
 }
 const unsigned ids[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13};
 for(unsigned id=0;id<14;id++)for(unsigned pos=0;pos<5;pos++)for(unsigned v=0;v<7;v++){
  memset(p,0x5a,sizeof p);memset(ram,0xa5,sizeof ram);p[pos].id=ids[id];
  unsigned d=ids[id]>=11?0x4700:0xf500+ids[id]*128;
  const unsigned from[]={0x19,0x1c,0x1f,0x22},to[]={0x1a,0x1d,0x20,0x23};
  for(unsigned i=0;i<4;i++)ram[d+from[i]]=(values[v]+i)&255;
  for(unsigned i=0;i<3;i++)ram16w(d+0x24+4*i,(values[v]+i)&65535);
  for(unsigned i=0;i<14;i++)ram[d+0x31+2*i]=(values[v]+i)&255;
  memcpy(want_p,p,sizeof p);memcpy(want_ram,ram,sizeof ram);
  want_p[pos].strength=ram[d+0x19];want_p[pos].mental=ram[d+0x1c];
  want_p[pos].agility=ram[d+0x1f];want_p[pos].dexterity=ram[d+0x22];
  for(unsigned i=0;i<4;i++)want_ram[d+to[i]]=ram[d+from[i]];
  want_p[pos].atk=ram16r(d+0x24);want_p[pos].def=ram16r(d+0x28);want_p[pos].magic_def=ram16r(d+0x2c);
  for(unsigned i=0;i<3;i++)put16(want_ram,d+0x26+4*i,ram16r(d+0x24+4*i));
  for(unsigned i=0;i<14;i++){want_p[pos].element_prop[i]=ram[d+0x31+2*i];want_ram[d+0x30+2*i]=ram[d+0x31+2*i];}
  for(unsigned repeat=0;repeat<2;repeat++){
   battle_restore_party_stats(&p[pos]);
   assert(!memcmp(p,want_p,sizeof p)&&!memcmp(ram,want_ram,sizeof ram));
  }
  restores++;
 }
 printf("{\"buff_transactions\":%u,\"restore_transactions\":%u,\"repeat_checks\":%u,\"whole_RAM_all_actor_guards\":true,\"ASan_UBSan\":true}\n",buffs,restores,buffs+restores);
}
'''
probe=O/'r46_cp26_enemy_stats_probe.c';exe=O/'r46_cp26_enemy_stats_probe';probe.write_text(code)
subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(probe),'-o',str(exe)],check=True)
r=subprocess.run([str(exe)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
assert r.returncode==0,r.stdout+r.stderr
report=json.loads(r.stdout)
report['native_enemy_stat_down_dispatch_covered_separately']=True
(O/'R46_CP26_ENEMY_STAT_RULES.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))

