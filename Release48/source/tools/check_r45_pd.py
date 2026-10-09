#!/usr/bin/env python3
"""Native half-HP selection and PD stat callback against original ROM and an
independent controller-stage interpreter. Palette and scroll controllers are
production; UI/hardware and enemy sprite-only overlays remain fixtures.
No claim of visual parity or a normal-save playthrough."""
from pathlib import Path
import re,subprocess,os,json,struct as binary
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts';prod=(R/'gba/title_runtime_v06.c').read_text()
def fn(name):
 m=re.search(r'^static[^\n]*\b'+name+r'\([^;]*?\)\s*\{',prod,re.M);assert m,name
 p=m.end();d=1
 while d:d+=(prod[p]=='{')-(prod[p]=='}');p+=1
 return prod[m.start():p]
struct=re.search(r'struct BattleLiteActor\s*\{.*?\};',prod,re.S).group()
s=r'''
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#define BATTLE_MAX_ENEMIES 4u
#include "precomp_assets.h"
#define BATTLE_ENEMY_ANIM_COMPONENTS 6u
static uint8_t rom[0x300000],battle_enemy_count,battle_enemy_placement[4],battle_enemy_reward_mask;
static uint8_t battle_enemy_anim_frame[4][6],battle_enemy_anim_timer[4][6],battle_enemy_reaction_flags[4];
static uint8_t battle_enemy_death_visible_mask,battle_enemy_death_pending_mask,battle_generic_pending_mask,battle_generic_stage_ready;
static uint8_t battle_enemy_ai_unknown_flag,battle_enemy_ai_hp25_gate,battle_enemy_last_regular_slot=255;
static int8_t battle_enemy_empty_side;static unsigned tick,rng_count;static FILE*trace;
static unsigned rom32(unsigned a);
static unsigned rom8(unsigned a){assert(a<sizeof rom);return rom[a];}static unsigned rom16(unsigned a){return rom8(a)*256+rom8(a+1);}
static unsigned rom32(unsigned a){return (rom16(a)<<16)|rom16(a+2);}
static unsigned field_npc_rng_next(void){return ++rng_count;}
static void ram8w(uint16_t a,uint8_t v){fprintf(trace,"cue,%u,%u,%u\n",tick,a,v);}
'''+struct+r'''
static void battle_enemy_anim_cache_bind(const struct BattleLiteActor*e,unsigned n){(void)e;assert(n==1);}

#define PANEL_ART_SCRATCH_SIZE 0x5000u
static uint32_t panel_storage[PANEL_ART_SCRATCH_SIZE/4];static uint8_t* const panel_art_scratch=(uint8_t*)panel_storage;
static uint8_t work_ram[65536],field_pause_fb[38400],battle_palette_hw_dirty;
static uint8_t battle_party_visible_mask=31,battle_party_pose_actor=255,battle_party_anim_attack,battle_party_x_override_actor=255;
static uint8_t battle_show_enemy_target,battle_damage_enemy_mask,battle_damage_party_mask;
static uint16_t field_pause_pal[256],native_main_frame_count;
static uint8_t battle_enemy_anim_cache_valid[4],battle_enemy_anim_cache_id[4],battle_enemy_anim_cache_ops[4];
static uint8_t battle_enemy_anim_cache_frames[4][6],battle_enemy_anim_cache_delay[4][6][128],battle_enemy_anim_cache_dst[4][6],battle_enemy_anim_cache_copy[4][6],battle_enemy_anim_cache_src[4][6][128];
static unsigned battle_compose_clip_active,battle_compose_clip_y0,battle_compose_clip_y1;static int battle_compose_clip_x0,battle_compose_clip_x1;
static uint16_t src_x[240],src_y[160];static unsigned observed_id;static FILE*states;
static void dma_fill32(void*d,unsigned n,uint32_t v){assert(!(n&3));uint32_t*q=d;for(unsigned i=0;i<n/4;i++)q[i]=v;}
static void dma_copy32(const void*s,void*d,unsigned n){assert(!(n&3));memcpy(d,s,n);}
static uint16_t md2gba(unsigned q){return ((q&14)*31/14)|((((q>>4)&14)*31/14)<<5)|((((q>>8)&14)*31/14)<<10);}
static void battle_compose_igg(const struct BattleLiteActor*,unsigned,const struct BattleLiteActor*,unsigned,unsigned);
static unsigned battle_src_y(unsigned row){return (row*40u)/7u;}
static void battle_compose_clip_set(int x0,unsigned y0,int x1,unsigned y1){battle_compose_clip_x0=x0;battle_compose_clip_y0=y0;battle_compose_clip_x1=x1;battle_compose_clip_y1=y1;battle_compose_clip_active=1u;}
static void battle_compose_clip_clear(void){battle_compose_clip_active=0u;}
static void battle_present_page0(void){tick++;native_main_frame_count++;}
static void battle_present_band(unsigned y0,unsigned y1){assert(y0<y1&&y1<=160u);tick++;native_main_frame_count++;}
static void battle_obj_sync(const struct BattleLiteActor*e,unsigned m){(void)e;(void)m;}
'''
for name in ['battle_art_pixel','battle_enemy_mapping_ops','battle_enemy_anim_descriptor','battle_enemy_native_meta','battle_enemy_assemble','battle_enemy_anim_advance_generic','battle_load_enemy','battle_enemy_alive_count','battle_enemy_alive_id','battle_enemy_ai_condition','battle_enemy_pick_skill']:s+=fn(name)+'\n'
from r46_pd_test_support import pd_span_support
s+=pd_span_support(prod,fn)
s+='#include "r45_pd_visual.h"\n'
s+=r'''
static void battle_compose_igg(const struct BattleLiteActor*p,unsigned n,const struct BattleLiteActor*e,unsigned t,unsigned a){(void)p;(void)n;(void)a;
 if(observed_id!=e[t].id){fprintf(trace,"replace,%u,%u,%u\n",tick,e[t].id,e[t].hp);observed_id=e[t].id;}
 R45PDVisual*s=r45_pd_visual();assert(fwrite(s->pal,2,32,states)==32&&fwrite(s->fg,2,192,states)==192&&fwrite(s->bg,2,192,states)==192);
 assert(r45_pd_background_compose(e));r45_pd_body_compose(e);r45_pd_palette_sync();
}
'''
s+='#include "r45_pd_battle.h"\n'
s+='__asm__(".section .rodata\\n.global ps4_precomp_assets\\nps4_precomp_assets:\\n.incbin \\\"'+str(R/'gba/precomp_assets.bin')+'\\\"\\n.global r45_pd_art_bank\\nr45_pd_art_bank:\\n.incbin \\\"'+str(R/'gba/r45_pd_art.bin')+'\\\"\\n.text\\n");\n'

s+=r'''
int main(int argc,char**argv){assert(argc==4);FILE*f=fopen(argv[1],"rb");assert(f&&fread(rom,1,sizeof rom,f)==sizeof rom);fclose(f);trace=fopen(argv[2],"w");assert(trace);states=fopen(argv[3],"wb");assert(states);for(unsigned x=0;x<240;x++)src_x[x]=x*320/240;for(unsigned y=0;y<160;y++)src_y[y]=y*224/160;
 struct BattleLiteActor p[5],e[4],before[5];memset(p,0x63,sizeof p);memcpy(before,p,sizeof p);battle_enemy_count=1;battle_enemy_placement[0]=0x14;battle_enemy_reward_mask=1;
 memset(work_ram,0xA5,sizeof work_ram);battle_load_enemy(0x85,e);r45_pd_visual_reset(e);observed_id=0x85;r45_pd_opening(p,5,e);assert(tick==192&&!memcmp(p,before,sizeof p)&&e[0].id==0x85);
 for(unsigned a=0;a<sizeof work_ram;a++)if(a<0xD000u||a>=0xE000u)assert(work_ram[a]==0xA5);
 for(unsigned id=0x85;id<=0x86;id++){
  battle_load_enemy(id,e);e[0].hp=e[0].max_hp/2+1;battle_enemy_last_regular_slot=255;rng_count=0;assert(battle_enemy_pick_skill(0,e,p,5)!=id-0x1E);
  e[0].hp=e[0].max_hp/2;assert(battle_enemy_pick_skill(0,e,p,5)==id-0x1E);
  tick=0;observed_id=id;memset(battle_enemy_anim_frame,3,sizeof battle_enemy_anim_frame);memset(battle_enemy_anim_timer,4,sizeof battle_enemy_anim_timer);battle_enemy_reaction_flags[0]=31;
  assert(r45_pd_transform(p,5,e,0,id-0x1E));fprintf(trace,"total,%u,%u\n",id,tick);
  assert(e[0].id==id+1&&e[0].hp==rom16(0x2816BC+(id+1)*48)&&e[0].max_hp==e[0].hp);
  assert(e[0].status==0&&battle_enemy_reaction_flags[0]==0&&battle_enemy_placement[0]==0x14&&battle_enemy_reward_mask==1&&!memcmp(p,before,sizeof p));
  assert(!r45_pd_visual()->controller&&r45_pd_visual()->body_id==id+1);
  /* Independent generic assembler checks every static and animated byte of
   * the resulting form. Sound/scratch is checked before the oracle runs. */
  for(unsigned a=0;a<sizeof work_ram;a++)if(a<0xD000u||a>=0xE000u)assert(work_ram[a]==0xA5);
  uint8_t*expected;const uint16_t*map;unsigned count,width,height;
  unsigned bytes=battle_enemy_assemble(0,id+1,&expected,&map,&count,&width,&height);
  assert(bytes==r45_pd_visual()->body_bytes&&!memcmp(expected,r45_pd_visual()->body_art,bytes));
  memset(work_ram,0xA5,sizeof work_ram);
 }
 tick=0;battle_load_enemy(0x87,e);assert(!r45_pd_transform(p,5,e,0,0x69)&&!tick);fclose(trace);fclose(states);puts("PASS PD half-HP actions, original-stat callbacks and unchanged party resources");}
'''
c=O/'r45_pd_probe.c';c.write_text(s);subprocess.run(['gcc','-std=c11','-O1','-g','-no-pie','-fsanitize=address,undefined','-I'+str(R/'gba'),str(c),'-o',str(O/'r45_pd_probe')],check=True)
trace=O/'R45_PD_TRACE.csv';subprocess.run([str(O/'r45_pd_probe'),str(R/'references/roms/ps4built.bin'),str(trace),str(O/"R45_PD_VISUAL_STATES.bin")],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
rows=[r.split(',') for r in trace.read_text().splitlines()];rom=(R/'references/roms/ps4built.bin').read_bytes()
expected=[];totals=[];callbacks=[]
for id,expand,table in [(0x85,168,0x2F70A),(0x86,192,0x2EB66)]:
 # Execute original five stage counters; pointer reaches a zero table sentinel
 # on its own update after the final callback, then the contraction counter
 # decrements through FF and the terminal palette counter wraps twice.
 stage=0;count=0;frame=0;ti=0;pt=0;terminal=None
 while True:
  if stage==0:
   pt=(pt+1)&7
   if not pt:
    count+=1
    if count==2:stage=1;count=0
  elif stage==1:
   count+=1
   if count==expand:stage=2;count=0
  elif stage==2:
   count=(count+1)&255;q=rom[table+ti]
   if not q:stage=3;count=expand-1
   elif count==q:
    if ti==6:callbacks.append((frame,id+1,int.from_bytes(rom[0x2816BC+(id+1)*48:0x2816BE+(id+1)*48],'big')))
    ti+=1
  elif stage==3:
   count=(count-1)&255
   if count==255:stage=4;pt=count=0
  elif stage==4:
   if terminal is None:terminal=frame
   pt=(pt+1)&7
   if not pt:
    count+=1
    if count==2:totals.append((id,frame+1));break
  frame+=1
 for f in range(terminal):
  if (f-(0 if id==0x85 else 1))%4==0:expected.append((f,0x500A,0xDD))
 expected.append((terminal,0x500A,0xED if id==0x85 else 0xA9))
 if id==0x86:expected.append((terminal,0x500B,0xED))
assert [tuple(map(int,r[1:])) for r in rows if r[0]=='replace']==callbacks
assert [tuple(map(int,r[1:])) for r in rows if r[0]=='total']==totals
assert [tuple(map(int,r[1:])) for r in rows if r[0]=='cue']==expected
report={'half_hp_transform_actions':[0x67,0x68],'source_stat_callbacks':callbacks,'source_controller_total_ticks':totals,'source_sound_writes':len(expected),'party_resources_preserved':True,'final_body_matches_independent_generic_assembly':True,'pd_art_does_not_write_md_work_or_sound_bytes':True,'scope':'ASan/UBSan production math, palettes, source scrolling and plane renderer. UI/hardware and sprite-only PLC D4/D9 overlays are fixtures; normal-save playthrough and GBA frame budget are unverified.'}
(O/'R45_PD_EVIDENCE.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS independent original controller clocks/callbacks/cues',totals)
