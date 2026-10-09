#!/usr/bin/env python3
"""Production selected-clip/background C; explicit VDP/idle/audio fixtures."""
from pathlib import Path
import json,os,subprocess
R=Path(__file__).resolve().parents[1];O=R.parent/'build_artifacts'
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define BATTLE_MAX_ENEMIES 4
struct BattleLiteActor {uint8_t id;uint16_t hp;};
struct BattleCommandLite {uint8_t type,id,target,inv_slot;};
static uint8_t battle_party_pose_actor=0xff,battle_party_pose_index=0xff;
static int battle_party_cast_pose_index(unsigned id){return id<11?3:-1;}
static void battle_present_action(const struct BattleLiteActor*p,unsigned n,const struct BattleLiteActor*e,unsigned et,int enemy,unsigned actor,const struct BattleCommandLite*c,unsigned combo){(void)p;(void)n;(void)e;(void)et;(void)enemy;(void)actor;(void)c;(void)combo;}
static uint32_t work_store[16384],vram_store[24576];
static uint16_t pal[256],opal[256],oam[512],field_pause_pal[256];
static uint8_t field_pause_fb[38400];
#define work_ram ((uint8_t*)work_store)
#define MODE4_VRAM16 ((uint16_t*)vram_store)
#define MODE4_PAGE1 ((uint16_t*)((uint8_t*)vram_store+0xA000))
#define OBJ_VRAM16 ((uint16_t*)((uint8_t*)vram_store+0x10000))
#define OAM16 oam
#define BG_PALETTE pal
#define OBJ_PALETTE opal
static unsigned battle_event_index,battle_enemy_count=4,battle_generic_pending_mask,battle_enemy_death_visible_mask;
static uint8_t battle_enemy_placement[4]={10,18,26,34};
static unsigned ps4_menu_base_cached,battle_palette_hw_dirty;
static unsigned ticks,idle_ticks,cues,cases,pixel_ticks,cur_tick,simulate_dirty,check_bg;
static const void* selected;
static void dma_copy32(const void*s,void*d,unsigned n){assert(!(n&3));memcpy(d,s,n);}
static void dma_copy16(const void*s,void*d,unsigned n){assert(!(n&1));memcpy(d,s,n);}
static void waitvb(void){
 if(battle_generic_pending_mask){
  if(simulate_dirty){unsigned x=100+(ticks%40);field_pause_fb[70*240+x]=192;memcpy(MODE4_VRAM16,field_pause_fb,38400);memcpy(pal,field_pause_pal,512);}
  battle_generic_pending_mask=0;
 }
 cur_tick=ticks++;
}
static void ram8w(unsigned a,unsigned v){assert(a==0x500a&&v);cues++;}
static void battle_action_idle_after_wait(struct BattleLiteActor* e);
#include "r46_battle_clips_runtime.h"
static void verify_pixels(void){
 if(!check_bg)return;const R46Clip*c=selected;const R46BgEv* ev=0;
 for(unsigned i=0;i<c->nbg;i++){const R46BgEv*q=&r46_bgev[c->first_bg+i];if(q->tick>cur_tick)break;ev=q;}
 const uint8_t* img=(const uint8_t*)MODE4_PAGE1;const uint8_t*out=(const uint8_t*)MODE4_VRAM16;
 const int8_t* sh=ev&&ev->kind==1&&ev->shift!=65535?r46_bg_shift+ev->shift*160u:0;
 for(int y=0;y<160;y++)for(int x=0;x<240;x++){
  unsigned orig=field_pause_fb[y*240+x],want=orig;
  if(ev&&ev->kind&&r46_bg_slot(orig)){
   if(ev->kind==2)want=79;
   else{int yy=y+ev->vshift,xx=x-(sh?sh[y]:0);if(yy<0)yy=0;if(yy>159)yy=159;if(xx<0)xx=0;if(xx>239)xx=239;want=img[yy*240+xx];}
  }
  assert(out[y*240+x]==want);
 }
 pixel_ticks++;
}
static void battle_action_idle_after_wait(struct BattleLiteActor* e){
 (void)e;verify_pixels();idle_ticks++;
 unsigned base=battle_event_index?512:768;
 for(unsigned i=64;i<112;i++)if(!(oam[i*4]&0x200)){
  assert((oam[i*4+2]&1023)>=base);assert((oam[i*4+1]>>9&31)>=16&&(oam[i*4+1]>>9&31)<20);
 }
 if(simulate_dirty&&ticks%3==0)battle_generic_pending_mask=1;
}
static void run_case(unsigned key,unsigned event,unsigned mask,unsigned fragmented,unsigned dirty){
 struct BattleLiteActor p[5]={{0,500},{1,500},{2,500},{3,500},{4,500}},e[4]={{132,500},{0,500},{0,500},{0,500}};
 for(unsigned i=0;i<5;i++)if(!(mask>>i&1))p[i].hp=0;
 const R46Clip*c=key<2?r46_clip_find(1,132,key?81:75):r46_clip_find(4,key==2?9:10,0);assert(c);
 selected=c;battle_event_index=event;ticks=idle_ticks=cues=0;simulate_dirty=dirty;check_bg=c->nbg!=0;battle_generic_pending_mask=0;
 memset(work_store,0,sizeof work_store);memset(vram_store,0,sizeof vram_store);memset(oam,2,sizeof oam);
 for(unsigned i=0;i<256;i++)field_pause_pal[i]=(uint16_t)(i*31u);
 for(unsigned y=0;y<160;y++)for(unsigned x=0;x<240;x++)field_pause_fb[y*240+x]=(uint8_t)(y>=120?110:(fragmented?(x%2?192:1):((x>=70&&x<90&&y>=60&&y<95)?192:1)));
 memcpy(MODE4_VRAM16,field_pause_fb,38400);memcpy(pal,field_pause_pal,512);
 uint8_t ps[sizeof p],es[sizeof e],before[16];memcpy(ps,p,sizeof p);memcpy(es,e,sizeof e);memset(work_ram+0xcff0,0xAB,16);memcpy(before,work_ram+0xcff0,16);memset(work_ram+0xe000,0xCD,16);
 uint8_t locked[8192];memset((uint8_t*)OBJ_VRAM16+512u*32u,0x51,8192);memcpy(locked,(uint8_t*)OBJ_VRAM16+512u*32u,8192);
 unsigned who[4]={0,1,2,3};int ok=key<2?r46_play_enemy_clip(p,5,e,0,key?81:75,0):r46_play_combo_clip(p,5,e,key==2?9:10,0,who,4);assert(ok);
 assert(ticks==c->ticks&&idle_ticks==c->ticks&&cues==c->nsfx);assert(!memcmp(ps,p,sizeof p)&&!memcmp(es,e,sizeof e));
 assert(!memcmp(before,work_ram+0xcff0,16));for(unsigned i=0;i<16;i++)assert(work_ram[0xe000+i]==0xCD);
 if(!event)assert(!memcmp(locked,(uint8_t*)OBJ_VRAM16+512u*32u,8192));
 if(c->nbg)assert(!memcmp(MODE4_VRAM16,field_pause_fb,38400));assert(!memcmp(pal,field_pause_pal,512));
 for(unsigned i=64;i<112;i++)assert(oam[i*4]&0x200);cases++;
}
static void command_case(unsigned ci,unsigned mask,unsigned event,unsigned actor){
 const R46Clip*c=&r46_clips[ci];assert(c->kind==9||c->kind==10);
 struct BattleLiteActor p[5]={{0,500},{1,500},{2,500},{3,500},{4,500}},e[4]={{0,500},{0,500},{0,500},{0,500}};
 for(unsigned i=0;i<4;i++)if(!(mask>>i&1))e[i].hp=0;
 struct BattleCommandLite cmd={(uint8_t)(c->kind==9?4:c->b?6:1),c->kind==9?c->a:c->b,0,0xff};
 unsigned pn=c->kind==10?1:5;if(c->kind==10){actor=0;p[0].id=(uint8_t)(c->a+10);}
 selected=c;ticks=idle_ticks=cues=0;check_bg=simulate_dirty=0;battle_generic_pending_mask=0;battle_event_index=event;
 memset(work_store,0xA5,sizeof work_store);memset(vram_store,0,sizeof vram_store);memset(oam,2,sizeof oam);
 memcpy(pal,field_pause_pal,512);memcpy(MODE4_VRAM16,field_pause_fb,38400);
 uint8_t before[sizeof p],eb[sizeof e],inv[40],uses[16];memcpy(before,p,sizeof p);memcpy(eb,e,sizeof e);memcpy(inv,work_ram+0xf410,40);memcpy(uses,work_ram+0x476a,16);
 battle_party_pose_actor=4;battle_party_pose_index=7;
 assert(cmd.type==1?r46_play_vehicle_attack(p,pn,e,3,actor,&cmd):r46_play_command_clip(p,pn,e,3,actor,&cmd));
 assert(ticks==c->ticks&&idle_ticks==c->ticks&&cues==c->nsfx);assert(!memcmp(before,p,sizeof p)&&!memcmp(eb,e,sizeof e));
 assert(!memcmp(inv,work_ram+0xf410,40)&&!memcmp(uses,work_ram+0x476a,16));
 assert(battle_party_pose_actor==4&&battle_party_pose_index==7);assert(!memcmp(pal,field_pause_pal,512));
 for(unsigned i=0;i<16;i++){assert(work_ram[0xcff0+i]==0xa5);assert(work_ram[0xe000+i]==0xa5);}
 for(unsigned i=64;i<112;i++)assert(oam[i*4]&0x200);cases++;
}
int main(void){
 for(unsigned key=0;key<2;key++)for(unsigned event=0;event<2;event++)for(unsigned mask=0;mask<32;mask++)run_case(key,event,mask,0,0);
 for(unsigned key=2;key<4;key++)for(unsigned event=0;event<2;event++)for(unsigned frag=0;frag<2;frag++)run_case(key,event,31,frag,1);
 struct BattleLiteActor pdp[5]={{0,100}},pde[4]={{0x85,100},{0x86,100},{0x87,100}};unsigned wh[4]={0};assert(!r46_play_combo_clip(pdp,5,pde,9,0,wh,1)&&!r46_play_combo_clip(pdp,5,pde,10,0,wh,1));
 for(unsigned ci=4;ci<r46_clip_count;ci++)for(unsigned mask=0;mask<16;mask++)for(unsigned event=0;event<2;event++){
  unsigned n=r46_clips[ci].kind==9?5:1;for(unsigned a=0;a<n;a++)command_case(ci,mask,event,a);
 }
 struct BattleCommandLite missing={4,0x7d,0,0xff};ticks=0;assert(!r46_play_command_clip(pdp,5,pde,0,0,&missing)&&ticks==0);
 for(unsigned frag=0;frag<2;frag++){
  for(unsigned i=0;i<38400;i++){field_pause_fb[i]=(uint8_t)(frag?(i%2?192:1):(i%43<9?192:1));((uint8_t*)MODE4_PAGE1)[i]=(uint8_t)(i%64);}
  for(unsigned i=0;i<256;i++)R46_SCR->isbg[i]=(uint8_t)(i<192&&r46_bg_slot(i));
  r46_bg_spans(R46_SCR->isbg);
  for(int vs=-127;vs<=127;vs+=127)for(unsigned sh=0;sh<32;sh++){
   R46BgEv ev={0,1,(int8_t)vs,0,(uint16_t)sh,0};memcpy(MODE4_VRAM16,field_pause_fb,38400);r46_bg_compose(&ev,R46_SCR->isbg,0);
   const int8_t*shift=r46_bg_shift+sh*160;
   for(int y=0;y<160;y++)for(int x=0;x<240;x++){unsigned want=field_pause_fb[y*240+x];if(r46_bg_slot(want)){int yy=y+vs,xx=x-shift[y];if(yy<0)yy=0;if(yy>159)yy=159;if(xx<0)xx=0;if(xx>239)xx=239;want=((uint8_t*)MODE4_PAGE1)[yy*240+xx];}assert(((uint8_t*)MODE4_VRAM16)[y*240+x]==want);}
  }
 }
 printf("PASS selected playback: %u cases, %u background ticks, all source ticks/idle/SFX retained, dead-target masks, VRAM windows, PD guards, background pixels/restoration and MD scratch guards\n",cases,pixel_ticks);
}
'''
f=O/'r46_recorded_runtime_probe.c';f.write_text(code)
a=O/'r46_recorded_runtime_banks.s';a.write_text('.section .rodata\n.balign 4\n.global r46_recorded_tables\nr46_recorded_tables:\n.incbin "'+str(R/'gba/r46_recorded_clips.bin')+'"\n.balign 4\n.global r46_tile_pool\nr46_tile_pool:\n.incbin "'+str(R/'gba/r46_recorded_tiles.bin')+'"\n.section .note.GNU-stack,"",@progbits\n')
exe=O/'r46_recorded_runtime_probe';p=subprocess.run(['gcc','-std=c11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I'+str(R/'gba'),str(f),str(a),str(R/'gba/r46_recorded_clips.c'),'-o',str(exe)],capture_output=True,text=True);assert p.returncode==0,p.stderr
p=subprocess.run([str(exe)],capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'));(O/'R46_RECORDED_RUNTIME_CHECK.log').write_text(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
(O/'R46_RECORDED_RUNTIME_EVIDENCE.json').write_text(json.dumps({'result':'PASS','output':p.stdout.strip(),'scope':'Production playback/background C under ASan/UBSan; hardware, DMA, native actor composition and sound output are fixtures. Raw source recording quality and GBA timing are not certified.'},indent=2)+'\n');print(p.stdout,end='')
