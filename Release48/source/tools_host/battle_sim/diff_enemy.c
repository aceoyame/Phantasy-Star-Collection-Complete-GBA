/* R15 byte-exact differential test: span kernel vs R14 per-pixel loop in
 * battle_draw_enemy, using the real precomp assets and the real source ROM image. */
#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PS4_BATTLE_ENEMY_SPAN_TEST 1
#include "title_host.c"
static void map_gba(void){
  unsigned long R[][2]={{0x02000000,0x40000},{0x03000000,0x8000},{0x04000000,0x1000},{0x05000000,0x1000},{0x06000000,0x18000},{0x07000000,0x1000}};
  for(unsigned i=0;i<6;i++){void* p=mmap((void*)R[i][0],R[i][1],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);if(p!=(void*)R[i][0]){fprintf(stderr,"map fail\n");exit(1);}}
}
static uint8_t ref[240*160],got[240*160],bg[240*160];
static unsigned long cases,pixels_written,mismatch_cases;
static void one(unsigned slot,unsigned id,int ex,int ey,unsigned line,int clip,int cx0,int cy0,int cx1,int cy1){
  if(clip)battle_compose_clip_set(cx0,(unsigned)cy0,cx1,(unsigned)cy1);else battle_compose_clip_clear();
  memcpy(field_pause_fb,bg,sizeof bg);battle_enemy_span_test_select=0;battle_draw_enemy(slot,id,ex,ey,line);memcpy(ref,field_pause_fb,sizeof ref);
  memcpy(field_pause_fb,bg,sizeof bg);battle_enemy_span_test_select=1;battle_draw_enemy(slot,id,ex,ey,line);memcpy(got,field_pause_fb,sizeof got);
  cases++;for(unsigned i=0;i<sizeof ref;i++)if(ref[i]!=bg[i])pixels_written++;
  if(memcmp(ref,got,sizeof ref)){if(mismatch_cases<5){unsigned d=0;for(unsigned i=0;i<sizeof ref;i++)if(ref[i]!=got[i])d++;fprintf(stderr,"MISMATCH id=%u slot=%u clip=%d diffpx=%u\n",id,slot,clip,d);}mismatch_cases++;}
}
int main(void){
  map_gba(); srand(7); for(unsigned i=0;i<sizeof bg;i++)bg[i]=(uint8_t)(rand()&0x3F);
  static const uint8_t place[4][4]={{0x14,0,0,0},{0x0E,0x1A,0,0},{0x0A,0x14,0x1E,0},{0x08,0x10,0x18,0x20}};
  unsigned ids=0;
  for(unsigned id=0;id<PS4_PRECOMP_BATTLE_ENEMY_NATIVE_COUNT;id++){
    const uint32_t* nm=battle_enemy_native_meta(id);if(!nm||!nm[6]||!nm[7])continue;ids++;
    for(unsigned count=1;count<=4;count++){
      static struct BattleLiteActor E[BATTLE_MAX_ENEMIES];memset(E,0,sizeof E);
      for(unsigned i=0;i<count;i++){E[i].id=(uint8_t)id;E[i].hp=100;E[i].max_hp=100;battle_enemy_placement[i]=(uint8_t)(place[count-1][i]|((i&1)?0x80:0));}
      battle_enemy_count=count;battle_enemy_death_visible_mask=0;for(unsigned i=0;i<BATTLE_MAX_ENEMIES;i++)battle_enemy_resolved_valid[i]=0;
      battle_enemy_anim_cache_bind(E,count);
      for(unsigned step=0;step<96;step++){
        unsigned ch=battle_enemy_anim_advance_generic(E);if(step&&!ch&&(step%24))continue;
        for(unsigned i=0;i<count;i++){int ex,ey;if(!battle_enemy_native_origin(i,id,&ex,&ey))continue;unsigned line=12u+((battle_enemy_placement[i]>>7)&1u);
          one(i,id,ex,ey,line,0,0,0,0,0);
          int x0=rand()%240,x1=x0+1+rand()%120,y0=rand()%160,y1=y0+1+rand()%80;one(i,id,ex,ey,line,1,x0&~3,y0,(x1+3)&~3,y1>160?160:y1);}
      }
      /* death dissolve masks */
      for(unsigned i=0;i<count;i++){battle_enemy_death_visible_mask|=1u<<i;for(unsigned t=0;t<64;t+=3){battle_enemy_death_timer[i]=(uint8_t)t;int ex,ey;if(battle_enemy_native_origin(i,id,&ex,&ey))one(i,id,ex,ey,12u,0,0,0,0,0);}}
    }
  }
  printf("enemy ids %u, draw cases %lu, reference pixels written %lu, mismatching cases %lu\n",ids,cases,pixels_written,mismatch_cases);
  return mismatch_cases?1:0;
}
