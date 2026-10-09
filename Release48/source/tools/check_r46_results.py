#!/usr/bin/env python3
"""Production reward/level arithmetic, drop bounds and debug abort probes.

Original ROM records supply independent expected values; hardware RNG words,
UI/audio and derived-stat side effects are fixtures. No full-save claim.
"""
from pathlib import Path
import json, os, re, subprocess
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
s=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(s[p]=='{')-(s[p]=='}');p+=1
 return s[m.start():p]+'\n'
actor=re.search(r'struct BattleLiteActor\s*\{.*?\};',s,re.S)[0]
result=re.search(r'struct BattleResultLite\s*\{.*?\};',s,re.S)[0]
pre=r'''#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define BATTLE_MAX_ENEMIES 4u
#define PS4_CHAR_BASE 0xF500u
#define PS4_INV_BASE 0xF410u
#define PS4_MONEY 0xF438u
static uint8_t work_ram[65536],rom[0x300000],battle_enemy_count,battle_enemy_reward_mask,battle_debug_battle_active;
static unsigned roll,rng_calls,derived_calls,messages,stops,plays;static uint8_t field_random_battle_delay,battle_random_escape_param;
static struct {uint8_t music;}field_map;
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}
static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}
static unsigned rom32(unsigned a){return (rom16(a)<<16)|rom16(a+2);}
static uint8_t ram8r(uint16_t a){return work_ram[a];}
static uint16_t ram16r(uint16_t a){return (ram8r(a)<<8)|ram8r(a+1);}
static uint32_t ram32r(uint16_t a){return ((uint32_t)ram16r(a)<<16)|ram16r(a+2);}
static void ram8w(uint16_t a,uint8_t v){work_ram[a]=v;}
static void ram16w(uint16_t a,uint16_t v){ram8w(a,v>>8);ram8w(a+1,v);}
static void ram32w(uint16_t a,uint32_t v){ram16w(a,v>>16);ram16w(a+2,v);}
static void update_one_char_derived(unsigned id){assert(id<11);derived_calls++;}
static unsigned ps4_rng2_next(void){rng_calls++;return roll;}
static void battle_ui_message(const char*a,const char*b){assert(!strcmp(a,"TEST ENDED")&&!strcmp(b,"NO REWARDS"));messages++;}
static void ps4_audio_stop(void){stops++;}
static void ps4_audio_play(unsigned n){assert(n==field_map.music);plays++;}
static void r46_zio_stop(void){} /* unrelated story-controller cleanup */
'''
body=actor+result+'\n'+''.join(fn(n) for n in ['battle_actor_incapacitated','battle_calc_chance','battle_try_escape','battle_char_record','ps4_inventory_add','battle_div_small','battle_apply_levelups_one','battle_apply_rewards','battle_exit_debug_test'])
main=r'''
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);
 unsigned cases=0;struct BattleLiteActor e[4],p[5];struct BattleResultLite r;
 for(unsigned eid=0;eid<153;eid++)for(unsigned count=1;count<=4;count++)for(unsigned alive=0;alive<=5;alive++){
  memset(work_ram,0,sizeof work_ram);memset(e,0,sizeof e);memset(p,0,sizeof p);memset(&r,0,sizeof r);
  for(unsigned i=0;i<count;i++)e[i].id=eid;
  battle_enemy_count=count;battle_enemy_reward_mask=(1u<<count)-1;
  for(unsigned i=0;i<5;i++){p[i].id=i;p[i].hp=i<alive?100:0;ram16w(0xF508+i*128,99);ram32w(0xF50A+i*128,9999990);}
  ram32w(PS4_MONEY,9999990);roll=127;rng_calls=0;battle_apply_rewards(e,p,5,&r);
  unsigned exp=rom16(0x2816BC+eid*48+44)*count,money=rom16(0x2816BC+eid*48+46)*count,share=alive?exp/alive:0;
  assert(r.exp_total==exp&&r.exp_share==share&&r.meseta==money&&rng_calls==1&&!r.item);
  assert(ram32r(PS4_MONEY)==(money>9?9999999:9999990+money));
  for(unsigned i=0;i<5;i++){unsigned add=i<alive?share:0;assert(ram32r(0xF50A+i*128)==(add>9?9999999:9999990+add));assert(!r.levels[i]);}
  cases++;
 }
 for(unsigned mask=0;mask<16;mask++){
  memset(work_ram,0,sizeof work_ram);memset(e,0,sizeof e);memset(p,0,sizeof p);memset(&r,0,sizeof r);
  unsigned exp=0,money=0;battle_enemy_count=4;battle_enemy_reward_mask=mask;
  for(unsigned i=0;i<4;i++){e[i].id=i*9;if(mask&(1u<<i)){exp+=rom16(0x2816BC+e[i].id*48+44);money+=rom16(0x2816BC+e[i].id*48+46);}}
  for(unsigned i=0;i<5;i++){p[i].id=i;p[i].hp=i==1?0:1;p[i].status=i==3?0x44:0;ram16w(0xF508+i*128,99);}
  roll=0;battle_apply_rewards(e,p,5,&r);assert(r.exp_total==exp&&r.exp_share==exp/3&&r.meseta==money);
  for(unsigned i=0;i<5;i++)assert(ram32r(0xF50A+i*128)==(i==1||i==3?0:exp/3));cases++;
 }
 const unsigned start[11]={1,7,1,17,6,1,12,20,25,27,35};
 const unsigned table[11]={0x2856B0,0x285F1C,0x286704,0x286F70,0x28767C,0x287E7A,0x2886E6,0x288E60,0x28952A,0x289B86,0x28A1B6};
 unsigned levels=0;
 for(unsigned id=0;id<11;id++)for(unsigned lv=start[id];lv<99;lv++){
  memset(work_ram,0,sizeof work_ram);unsigned d=0xF500+id*128,a=table[id]+(lv-start[id])*22,target=rom32(a);
  ram16w(d+8,lv);ram32w(d+10,target-1);assert(!battle_apply_levelups_one(id));
  ram32w(d+10,target);for(unsigned j=0;j<8;j++)ram8w(d+0x6A+j*2,7);
  unsigned n=battle_apply_levelups_one(id);assert(n==1&&ram16r(d+8)==lv+1);
  assert(ram16r(d+0x10)==rom16(a+4)&&ram16r(d+0x14)==rom16(a+6));
  assert(ram8r(d+0x18)==rom8(a+8)&&ram8r(d+0x1B)==rom8(a+9)&&ram8r(d+0x1E)==rom8(a+10)&&ram8r(d+0x21)==rom8(a+11));
  assert(ram8r(d+0x52)==rom8(a+12)&&ram8r(d+0x62)==rom8(a+13));
  for(unsigned j=0;j<8;j++){assert(ram8r(d+0x6B+j*2)==rom8(a+14+j));assert(ram8r(d+0x6A+j*2)==(j==0&&rom8(a+13)?rom8(a+14):7));}levels++;
 }
 memset(e,0,sizeof e);memset(p,0,sizeof p);battle_enemy_count=1;battle_enemy_reward_mask=1;p[0].id=0;p[0].hp=1;
 for(unsigned full=0;full<2;full++)for(unsigned outcome=0;outcome<2;outcome++){
  memset(work_ram,0,sizeof work_ram);memset(&r,0,sizeof r);ram16w(0xF508,99);
  memset(work_ram+PS4_INV_BASE,0x91,40);if(!full)ram8w(PS4_INV_BASE+17,0);
  ram8w(0x41F2,1);ram8w(0x41F3,0x4E);roll=outcome?0:1;
  battle_apply_rewards(e,p,1,&r);assert(r.item==(outcome?0x4E:0));assert(r.item_full==(full&&outcome));
  assert(ram8r(PS4_INV_BASE+17)==(full?0x91:outcome?0x4E:0));
 }
 unsigned run_cases=0,failed=0,passed=0;
 for(unsigned agility=0;agility<256;agility++)for(unsigned param=0;param<256;param+=16)for(unsigned word=0;word<64;word++){
  memset(p,0,sizeof p);p[0].hp=1;p[0].agility=agility;p[1].hp=1;p[1].agility=255;p[1].status=0x04;
  roll=word;rng_calls=0;battle_random_escape_param=param;
  int expected=agility&&param<240&&2*((int)word+(int)agility-(int)param)>40;
  int actual=battle_try_escape(p,2);assert(actual==expected);assert(rng_calls==(agility&&param<240));
  run_cases++;if(actual)passed++;else failed++;
 }
 assert(passed&&failed);
 memset(work_ram,0xA5,sizeof work_ram);unsigned char before[65536];memcpy(before,work_ram,sizeof before);
 field_map.music=0x8F;assert(!battle_exit_debug_test()&&!messages&&!stops&&!plays);
 battle_debug_battle_active=1;assert(battle_exit_debug_test());assert(messages==1&&stops==1&&plays==1&&field_random_battle_delay==10);assert(!memcmp(before,work_ram,sizeof before));
 printf("PASS %u reward distributions/caps, %u source level thresholds, four item-drop/free/full cases, %u RUN cases (%u fail/%u success), debug abort without RAM writes\n",cases,levels,run_cases,failed,passed);
}
'''
(O/'r46_results_probe.c').write_text(pre+body+main)
subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined',str(O/'r46_results_probe.c'),'-o',str(O/'r46_results_probe')],check=True)
p=subprocess.run([str(O/'r46_results_probe'),str(R/'references/roms/ps4built.bin')],check=True,text=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
report={'result':'PASS','output':p.stdout.strip(),'scope':__doc__,'full_inventory_replacement_dialogue_in_this_probe':False,'normal_save_or_hardware_verified':False}
(O/'R46_RESULTS_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print(p.stdout,end='')
