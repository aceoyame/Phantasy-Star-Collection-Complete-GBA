#!/usr/bin/env python3
"""Actual CP28 turn-end C with deterministic original-source oracle and guards."""
from pathlib import Path
import json,re,subprocess,tempfile,os,argparse
ap=argparse.ArgumentParser();ap.add_argument('--label',default='CP28');a=ap.parse_args()
S=Path(__file__).resolve().parents[1];O=S.parent/'build_artifacts'
text=(S/'gba/title_runtime_v06.c').read_text()
start=text.index('static void battle_restore_stats_at_turn_end(')
end=text.index('\nstatic void battle_resolve_mixed_round',start)
production=text[start:end]
probe=r'''
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define BATTLE_MAX_ENEMIES 4u
struct BattleLiteActor {uint16_t hp;uint8_t status,agility,dexterity,base_agility,base_dexterity,element_prop[14],base_element_prop[14],guard[17];};
static unsigned battle_enemy_count,rolls,bit;
static uint16_t ps4_rng2_next(void){rolls++;return (uint16_t)bit;}
static uint32_t field_npc_rng_next(void){abort();}
PRODUCTION
static void original(struct BattleLiteActor p[5],unsigned pn,struct BattleLiteActor e[4],unsigned en,unsigned *calls){
 if(pn>5)return;
 for(unsigned i=0;i<pn;i++){
  p[i].element_prop[0]=p[i].base_element_prop[0];
  if(p[i].status&0x28){(*calls)++;if(bit){p[i].status&=0xd7;p[i].agility=p[i].base_agility;}}
 }
 for(unsigned i=0;i<en&&i<4;i++)if(e[i].hp){
  e[i].element_prop[0]=e[i].base_element_prop[0];
  if(e[i].status&0x28){(*calls)++;if(bit){e[i].status&=0xd7;e[i].agility=e[i].base_agility;}}
  e[i].status&=0xfd;
 }
}
int main(void){
 struct Guarded {uint8_t before[32];struct BattleLiteActor p[5],e[4];uint8_t after[32];} x,w;
 unsigned cases=0;
 for(unsigned st=0;st<256;st++)for(bit=0;bit<2;bit++){
  memset(&x,0xa5,sizeof x);
  for(unsigned i=0;i<9;i++){
   struct BattleLiteActor *v=i<5?&x.p[i]:&x.e[i-5];
   v->hp=i%3?100:0;v->status=(uint8_t)(st+i*13);v->agility=1;v->dexterity=2;v->base_agility=79;v->base_dexterity=93;
   v->element_prop[0]=1;v->base_element_prop[0]=4;
  }
  w=x;rolls=0;unsigned want_calls=0;battle_enemy_count=4;
  original(w.p,5,w.e,4,&want_calls);battle_restore_stats_at_turn_end(x.p,5,x.e);
  if(memcmp(&x,&w,sizeof x)||rolls!=want_calls)abort();
  cases++;
 }
 for(unsigned pn=0;pn<8;pn++)for(unsigned en=0;en<7;en++){
  memset(&x,0xa5,sizeof x);
  for(unsigned i=0;i<5;i++){x.p[i].hp=100;x.p[i].status=0x2a;}
  for(unsigned i=0;i<4;i++){x.e[i].hp=100;x.e[i].status=0x2a;}
  w=x;rolls=0;bit=1;unsigned want_calls=0;battle_enemy_count=en;
  original(w.p,pn,w.e,en,&want_calls);battle_restore_stats_at_turn_end(x.p,pn,x.e);
  if(memcmp(&x,&w,sizeof x)||rolls!=want_calls)abort();
  cases++;
 }
 printf("PASS %u guarded source turn-end transactions\n",cases);return 0;
}
'''.replace('PRODUCTION',production)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'probe.c').write_text(probe)
 subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'probe.c'),'-o',str(p/'probe')],check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0'
 done=subprocess.run([str(p/'probe')],check=True,text=True,capture_output=True,env=env);print(done.stdout,end='')
(O/f'R46_{a.label}_TURN_END_RULES.json').write_text(json.dumps({'source':'actual final battle_restore_stats_at_turn_end C','oracle':'original $67DE sleep/physical/paralysis rules; native HP-zero enemy occupancy adaptation','compiler':'GCC ASan/UBSan','transactions':568,'all_256_status_bytes_both_wake_results':True,'whole_actor_and_surrounding_guards':True,'pn_and_enemy_count_bounds_checked':True,'field_RNG_calls_abort':True,'all_pass':True},indent=2)+'\n')

