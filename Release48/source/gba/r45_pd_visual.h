/* Source PD plane/palette controller. The panel workspace is idle in battle;
 * no extra persistent RAM and no writes to native field chunk tables. */
#include "r45_pd_art.h"
#include "r46_pd_span_plans.h"
typedef struct __attribute__((aligned(4))) {
 uint16_t pal[32],saved[32];int16_t fg[192],bg[192];
 uint8_t bg_art[6400],body_art[9216];
 uint16_t bg_bytes,body_bytes,source_bytes,queue_at,pal_clock;
 uint8_t active,controller,bg_map,body_id,bg_id,angle,anim_id,queue_kind,queue_id,bg_angle;
 uint8_t cycle_frame[12],cycle_timer[12],cycle_on[12];
 /* Resident party band, used only during synchronous PD controllers. */
 int16_t resident_bg[34];uint8_t resident_valid,resident_map,resident_party_mask;
} R45PDVisual;
typedef char R45PDWorkspaceFits[(sizeof(R45PDVisual)<=PANEL_ART_SCRATCH_SIZE)?1:-1];
static R45PDVisual* r45_pd_visual(void){return (R45PDVisual*)(void*)panel_art_scratch;}
/* Only the dedicated PD plane controller owns the native battle-object arena
 * while it runs. Keep its immutable run plans and current body map in that
 * arena, rather than rereading thousands of small cartridge records per tick.
 * Field chunk RAM ($6000) and Sound_Index ($500A/$500B) stay untouched. */
#define R46_PD_CACHE_TAG 0xDFFCu
#define R46_PD_BG_CACHE_TAG 0xDFF8u
#define R46_PD_BG_CACHE_OFF 0x4400u
typedef char R46PDBGCacheFits[(sizeof(R45PDVisual)<=R46_PD_BG_CACHE_OFF&&R46_PD_BG_CACHE_OFF+3072u<=PANEL_ART_SCRATCH_SIZE)?1:-1];
static const struct R46PDRun* r46_pd_run_plan(unsigned phase){
 if(r45_pd_visual()->controller)return (const struct R46PDRun*)(const void*)(work_ram+0xD000u)+phase*41u;
 return r46_pd_runs[phase];
}
static const uint16_t* r46_pd_body_map(unsigned id){
 const uint32_t*n=battle_enemy_native_meta(id);uint32_t tag=0x50443100u|id;
 const uint16_t*map=(const uint16_t*)(const void*)(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_MAP_OFF+n[4]);
 if(!r45_pd_visual()->controller)return map;
 if(*(const uint32_t*)(const void*)(work_ram+R46_PD_CACHE_TAG)!=tag){
  dma_copy32(r46_pd_runs,work_ram+0xD000u,sizeof(r46_pd_runs));
  dma_copy32(map,work_ram+0xDA40u,1200u);
  *(uint32_t*)(void*)(work_ram+R46_PD_CACHE_TAG)=tag;
 }
 return (const uint16_t*)(const void*)(work_ram+0xDA40u);
}
static const uint16_t* r46_pd_background_map(unsigned id){
 const uint16_t*map=(const uint16_t*)(const void*)(r45_pd_art_bank+r45_pd_bg_meta[id][2]);
 if(!r45_pd_visual()->controller)return map;
 uint32_t tag=0x50444200u|id;
 if(*(const uint32_t*)(const void*)(work_ram+R46_PD_BG_CACHE_TAG)!=tag){
  dma_copy32(map,panel_art_scratch+R46_PD_BG_CACHE_OFF,3072u);
  *(uint32_t*)(void*)(work_ram+R46_PD_BG_CACHE_TAG)=tag;
 }
 return (const uint16_t*)(const void*)(panel_art_scratch+R46_PD_BG_CACHE_OFF);
}
static void r46_pd_cache_retire(void){*(uint32_t*)(void*)(work_ram+R46_PD_CACHE_TAG)=0u;*(uint32_t*)(void*)(work_ram+R46_PD_BG_CACHE_TAG)=0u;}
static int r45_pd_formation(const struct BattleLiteActor* e){return battle_enemy_count==1u&&e&&e[0].id>=0x85u&&e[0].id<=0x87u;}
static void r45_pd_palette_sync(void){R45PDVisual*s=r45_pd_visual();for(unsigned i=0;i<16;i++){field_pause_pal[208u+i]=md2gba(s->pal[i]);field_pause_pal[192u+i]=md2gba(s->pal[16u+i]);}battle_palette_hw_dirty=1u;}
static void r45_pd_bg_load(unsigned id){R45PDVisual*s=r45_pd_visual();unsigned n=id-0x85u;s->bg_map=s->bg_id=(uint8_t)n;s->bg_bytes=(uint16_t)r45_pd_bg_meta[n][1];dma_fill32(s->bg_art,sizeof(s->bg_art),0u);dma_copy32(r45_pd_art_bank+r45_pd_bg_meta[n][0],s->bg_art,s->bg_bytes);}
static void r45_pd_visual_reset(const struct BattleLiteActor*e){
 R45PDVisual*s=r45_pd_visual();s->active=0u;if(!r45_pd_formation(e))return;
 dma_fill32(s,sizeof(*s),0u);s->active=1u;s->body_id=s->anim_id=e[0].id;s->pal_clock=native_main_frame_count;r45_pd_bg_load(e[0].id);
 for(unsigned i=0;i<32;i++)s->pal[i]=s->saved[i]=rom16(0x306F4u+i*2u);
 if(e[0].id==0x85u)for(unsigned i=0;i<14;i++)s->pal[i]=s->pal[16u+i]=0xEEEu;
 else for(unsigned i=0;i<16;i++)s->pal[16u+i]=s->saved[16u+i]=rom16((e[0].id==0x86u?0x2F898u:0x2ED98u)+i*2u);
}
/* loc_27E238 and loc_27E1E0/27C: word arithmetic, including carries/borrows
 * between nibbles, follows the three rotate-and-add source passes. */
static uint16_t r45_pd_pal_step(uint16_t value,uint16_t target,int delta,int zero){
 for(unsigned sh=4u;sh<=8u;sh+=4u){uint16_t q=(uint16_t)((value>>sh)|(value<<(16u-sh))),t=(uint16_t)((target>>sh)|(target<<(16u-sh)));if(zero?(q&15u)!=0u:(q&15u)!=(t&15u))q=(uint16_t)(q+delta);value=(uint16_t)((q<<sh)|(q>>(16u-sh)));}
 if(zero?(value&15u)!=0u:(value&15u)!=(target&15u))value=(uint16_t)(value+delta);return value;
}
static void r45_pd_pal_range(unsigned start,int delta,int zero){R45PDVisual*s=r45_pd_visual();for(unsigned i=1;i<=13;i++)s->pal[start+i]=r45_pd_pal_step(s->pal[start+i],s->saved[start+i],delta,zero);}
static int r45_pd_cos(unsigned a){unsigned q=(a+0x40u)&255u;return q&128u?-(int)rom8(0x423CCu+((~q)&127u)):(int)rom8(0x423CCu+q);}
/* Four angle units per scanline repeat after 64 lines. Preserve the source
 * signed shift, then reuse that exact period for the remaining 56 lines. */
static void r46_pd_background_scroll(unsigned angle){
 R45PDVisual*s=r45_pd_visual();
 for(unsigned i=0;i<64;i++)s->bg[i]=(int16_t)(r45_pd_cos(angle+i*4u)>>3);
 for(unsigned i=0;i<56;i++)s->bg[64u+i]=s->bg[i];
}
static void r45_pd_cycles_load(void){R45PDVisual*s=r45_pd_visual();for(unsigned i=0;i<9;i++){s->cycle_on[i]=1u;s->cycle_frame[i]=(uint8_t)(i<6u?i:0u);s->cycle_timer[i]=0u;}}
static void r45_pd_palette_tick(void){
 R45PDVisual*s=r45_pd_visual();unsigned elapsed=(uint16_t)(native_main_frame_count-s->pal_clock);s->pal_clock=native_main_frame_count;
 for(unsigned t=0;t<elapsed;t++)for(unsigned i=0;i<12;i++)if(s->cycle_on[i]){
  unsigned table=i<6u?0x30600u:i==6u?0x3066Eu:i==7u?0x30694u:i==8u?0x306BAu:0x2ED36u+(i-9u)*20u;
  unsigned f=s->cycle_frame[i];if(s->cycle_timer[i]>=rom8(table+1u+f)){s->cycle_timer[i]=0u;f++;}s->cycle_timer[i]++;
  unsigned n=rom8(table);if(f>=n)f=0u;s->cycle_frame[i]=(uint8_t)f;unsigned colors=table+1u+n+((n&1u)?0u:1u);
  s->pal[i<9u?3u+i:26u+i-9u]=rom16(colors+f*2u);
 }
}
static void r45_pd_open_tick(unsigned f){
 R45PDVisual*s=r45_pd_visual();
 if(f<64u&&(f&7u)==7u&&f<63u){r45_pd_pal_range(0u,-2,0);r45_pd_pal_range(16u,-2,0);if(s->pal[0])s->pal[0]=(uint16_t)(s->pal[0]-0x222u);}
 if(f==63u)r45_pd_cycles_load();
 if(f==191u){for(unsigned i=0;i<192;i++)s->fg[i]=s->bg[i]=0;r46_pd_background_scroll(f);return;}
 s->angle=(uint8_t)(s->angle+2u);unsigned diminish=f<64u?0u:f-63u;
 for(unsigned i=0;i<64;i++){int v=r45_pd_cos(s->angle+i*4u);if(v>0){v-=(int)diminish;if(v<0)v=0;}else{v+=(int)diminish;if(v>0)v=0;}s->fg[i]=(int16_t)v;}
 for(unsigned i=0;i<56;i++)s->fg[64u+i]=s->fg[i];
 r46_pd_background_scroll(f);
 for(unsigned i=0;i<72;i++)s->bg[120u+i]=s->fg[i];
}
static void r45_pd_expand(unsigned n){R45PDVisual*s=r45_pd_visual();unsigned top=n<120u?n:120u;for(unsigned i=0;i<top;i++)s->fg[i]=(int16_t)(s->fg[i]+(int16_t)rom16(0x2FA1Eu+i*2u));if(n>120u)for(unsigned i=0;i<n-120u;i++)s->bg[120u+i]=(int16_t)(s->bg[120u+i]+(int16_t)rom16(0x2FA1Eu+i*2u));}
static void r45_pd_contract(unsigned n,unsigned maximum){R45PDVisual*s=r45_pd_visual();if(n==255u)return;r45_pd_expand(n+1u);if(n>119u)s->bg[n+1u]=0;else if(n==119u)s->bg[120u]=0;else s->fg[n+1u]=0;(void)maximum;}
static void r45_pd_new_palette(unsigned old){R45PDVisual*s=r45_pd_visual();unsigned live=old==0x85u?0x2F8B8u:0x2EDB8u,save=old==0x85u?0x2F898u:0x2ED98u;for(unsigned i=0;i<16;i++){s->pal[16u+i]=rom16(live+i*2u);s->saved[16u+i]=rom16(save+i*2u);}}
static unsigned r45_pd_pixel(const uint8_t*art,unsigned bytes,unsigned e,unsigned x,unsigned y){unsigned tile=e&0x7FFu;if(e&0x800u)x=7u-x;if(e&0x1000u)y=7u-y;return battle_art_pixel(art,bytes,tile,x,y);}
static void r46_comb_odd(volatile uint16_t*,const volatile uint16_t*,unsigned);
#if defined(__arm__)
/* The ready background pool is immutable cartridge data. Aligned word reads
 * span each odd byte start; only requested bytes are stored. A final load can
 * include up to three unused neighboring ROM bytes. Never use this kernel
 * for volatile/IO sources or an unaligned destination. */
__attribute__((naked,section(".iwram.text"),aligned(4)))
static void r46_pd_copy_odd_words(uint8_t*dst,const uint8_t*src,unsigned words){
 /* Four consecutive aligned cartridge words share one load/store sequence.
  * Keep the final source word for the next block and use the exact old tail
  * for 1..3 words. The immutable source may include unused neighbor bytes. */
 __asm__ volatile("push {r4-r8}\n and r3,r1,#3\n mov r3,r3,lsl #3\n rsb ip,r3,#32\n bic r1,r1,#3\n ldr r4,[r1],#4\n cmp r2,#4\n blo 2f\n"
 "1: ldmia r1!,{r5-r8}\n mov r4,r4,lsr r3\n orr r4,r4,r5,lsl ip\n mov r5,r5,lsr r3\n orr r5,r5,r6,lsl ip\n mov r6,r6,lsr r3\n orr r6,r6,r7,lsl ip\n mov r7,r7,lsr r3\n orr r7,r7,r8,lsl ip\n stmia r0!,{r4-r7}\n mov r4,r8\n sub r2,r2,#4\n cmp r2,#4\n bhs 1b\n"
 "2: cmp r2,#0\n beq 4f\n 3: ldr r5,[r1],#4\n mov r4,r4,lsr r3\n orr r4,r4,r5,lsl ip\n str r4,[r0],#4\n mov r4,r5\n subs r2,r2,#1\n bne 3b\n 4: pop {r4-r8}\n bx lr\n");
}
#else
static void r46_pd_copy_odd_words(uint8_t*dst,const uint8_t*src,unsigned words){memcpy(dst,src,words*4u);}
#endif
#if defined(__arm__)
__attribute__((target("thumb"),noinline,aligned(4)))
#endif
static void r46_pd_copy_span(uint8_t* dst,const uint8_t* src,unsigned n){
 if(!n)return;if((uintptr_t)dst&1u){*dst++=*src++;--n;}
 unsigned pairs=n>>1;
 if(pairs&&((uintptr_t)src&1u)){
  if((uintptr_t)dst&2u){dst[0]=src[0];dst[1]=src[1];dst+=2u;src+=2u;--pairs;}
  unsigned words=pairs>>1;if(words)r46_pd_copy_odd_words(dst,src,words);
  dst+=words*4u;src+=words*4u;if(pairs&1u){dst[0]=src[0];dst[1]=src[1];dst+=2u;src+=2u;}
 }else{if(pairs)dma_copy16(src,dst,pairs*2u);dst+=pairs*2u;src+=pairs*2u;}
 if(n&1u)*dst=*src;
}
static void r46_pd_blank_span(uint8_t*dst,unsigned n){
 while(n&&((uintptr_t)dst&3u)){*dst++=208u;--n;}
 unsigned words=n&~3u;if(words)dma_fill32(dst,words,0xD0D0D0D0u);
 for(unsigned i=words;i<n;i++)dst[i]=208u;
}
/* Each source run carries six
 * packed 5-bit shifts, avoiding six cartridge reads and ROM loop fetches.
 * Transparent nibbles preserve the already composed background exactly. */
#if defined(__arm__)
__attribute__((naked,section(".iwram.text"),aligned(4)))
static void r46_pd_nibble_span(uint32_t bits,uint32_t shifts,uint8_t*dst,unsigned count_base){
 __asm__ volatile("push {r4,lr}\n mov r4,r3,lsr #8\n and r3,r3,#255\n 1: and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r0,lsr ip\n ands ip,ip,#15\n orrne ip,ip,r4\n strbne ip,[r2]\n add r2,r2,#1\n subs r3,r3,#1\n bne 1b\n pop {r4,lr}\n bx lr\n");
}
#else
static void r46_pd_nibble_span(uint32_t bits,uint32_t shifts,uint8_t*dst,unsigned count_base){unsigned base=count_base>>8,n=count_base&255u;for(unsigned i=0;i<n;i++){unsigned c=(bits>>(shifts&31u))&15u;shifts>>=5;if(c)dst[i]=(uint8_t)(base+c);}}
#endif
typedef uint32_t pd_u32_alias __attribute__((may_alias));
#if defined(__arm__)
__attribute__((target("thumb"),noinline,aligned(4)))
#endif
static void r46_pd_partial_bg_row(const uint16_t*m,unsigned sy,int shift,unsigned x0,unsigned x1,uint8_t*dst){
 R45PDVisual*s=r45_pd_visual();unsigned start=(unsigned)(-shift)&511u,phase=start&7u,base_tx=start>>3;
 const struct R46PDRun*plan=r46_pd_run_plan(phase);
 r46_pd_blank_span(dst+x0,x1-x0);
 for(unsigned r=0;r<r46_pd_nruns[phase];r++){
  const struct R46PDRun*p=&plan[r];unsigned x=p->x,end=x+p->n;if(x<x0)x=x0;if(end>x1)end=x1;if(end<=x)continue;
  unsigned tx=(base_tx+p->tx)&63u,e=sy<192u?m[(sy>>3)*64u+tx]:0u,tile=e&2047u;if(tile*32u+32u>s->bg_bytes)continue;
  unsigned ly=sy&7u;if(e&0x1000u)ly=7u-ly;uint32_t bits=*(const pd_u32_alias*)(const void*)(s->bg_art+tile*32u+ly*4u);if(!bits)continue;
  uint32_t shifts=p->shifts;if(e&0x800u)shifts^=0x39CE739Cu;shifts>>=(x-p->x)*5u;
  r46_pd_nibble_span(bits,shifts,dst+x,(end-x)|((e&0x2000u?192u:208u)<<8));
 }
}
#if defined(__arm__)
__attribute__((target("thumb"),noinline,aligned(4)))
#endif
static void r45_pd_background(void){
 R45PDVisual*s=r45_pd_visual();const uint16_t*m=r46_pd_background_map(s->bg_map);
 unsigned y0=battle_compose_clip_active?battle_compose_clip_y0:0u,y1=battle_compose_clip_active?battle_compose_clip_y1:160u;
 unsigned x0=battle_compose_clip_active?(unsigned)battle_compose_clip_x0:0u,x1=battle_compose_clip_active?(unsigned)battle_compose_clip_x1:240u;
 const uint16_t*ids=(const uint16_t*)(const void*)(r45_pd_art_bank+r45_pd_bg_meta[s->bg_map][3]);int ready=s->bg_id==s->bg_map;
 for(unsigned y=y0;y<y1;y++){
  unsigned sy=src_y[y];int shift=sy<192u?s->bg[sy]:0;unsigned x=x0;uint8_t*dst=field_pause_fb+y*240u;
  if(sy>=192u&&ready){r46_pd_blank_span(dst+x0,x1-x0);}
  else if(sy<192u&&ready){
   unsigned phase=(unsigned)shift&511u;
   const uint8_t*row=r45_pd_art_bank+R46_PD_ROWS_OFF+(unsigned)ids[(phase&3u)*192u+sy]*384u;
   unsigned at=384u-(phase>>2)*3u+x0;if(at>=384u)at-=384u;
   unsigned n=x1-x0,first=384u-at;if(first>n)first=n;
   r46_pd_copy_span(dst+x0,row+at,first);if(first<n)r46_pd_copy_span(dst+x0+first,row,n-first);
  }else r46_pd_partial_bg_row(m,sy,shift,x0,x1,dst);
 }
}
typedef struct R46PDRowJob{const struct R46PDRun*plan;unsigned base_tx,ly,bytes,nruns;} R46PDRowJob;
#if defined(__arm__)
__attribute__((naked,section(".iwram.text"),aligned(4)))
static void r46_pd_body_row(const uint16_t*map,const uint8_t*art,uint8_t*dst,const R46PDRowJob*job){
 /* The caller trims each interval to consecutive valid map columns.
  * Advance its map pointer once per tile, avoiding repeated index/wrap work. */
 __asm__ volatile(
  "push {r4-r11,lr}\n mov r4,r0\n mov r5,r1\n mov r6,r2\n ldmia r3,{r7-r11}\n add r11,r7,r11,lsl #3\n ldr lr,9f\n"
  "1: ldr r0,[r7],#4\n ldr r1,[r7],#4\n ldrh r2,[r4],#2\n"
  "bic r3,r2,#0xf800\n mov r3,r3,lsl #5\n add ip,r3,#32\n cmp ip,r10\n bhi 3f\n mov ip,r9\n tst r2,#0x1000\n eorne ip,ip,#7\n add r3,r3,ip,lsl #2\n ldr r3,[r5,r3]\n cmp r3,#0\n beq 3f\n"
  "tst r2,#0x800\n eorne r1,r1,lr\n and ip,r0,#255\n add r2,r6,ip\n mov r0,r0,lsr #8\n and r0,r0,#255\n"
  /* Runs contain 1..6 projected pixels. Enter the corresponding suffix of
   * six equal-size blocks, biasing dst so its first store remains at x.
   * ARM PC reads the current instruction plus eight; the skipped nop makes
   * block zero exactly that base. No pixel-count branch or dst increment. */
  "rsb r0,r0,#6\n sub r2,r2,r0\n add r0,r0,r0,lsl #1\n add pc,pc,r0,lsl #3\n nop\n"
  "and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r3,lsr ip\n ands ip,ip,#15\n orrne ip,ip,#192\n strbne ip,[r2,#0]\n"
  "and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r3,lsr ip\n ands ip,ip,#15\n orrne ip,ip,#192\n strbne ip,[r2,#1]\n"
  "and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r3,lsr ip\n ands ip,ip,#15\n orrne ip,ip,#192\n strbne ip,[r2,#2]\n"
  "and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r3,lsr ip\n ands ip,ip,#15\n orrne ip,ip,#192\n strbne ip,[r2,#3]\n"
  "and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r3,lsr ip\n ands ip,ip,#15\n orrne ip,ip,#192\n strbne ip,[r2,#4]\n"
  "and ip,r1,#31\n mov r1,r1,lsr #5\n mov ip,r3,lsr ip\n ands ip,ip,#15\n orrne ip,ip,#192\n strbne ip,[r2,#5]\n"
  "3: cmp r7,r11\n blo 1b\n pop {r4-r11,lr}\n bx lr\n 9: .word 0x39ce739c\n");
}
#else
static void r46_pd_body_row(const uint16_t*map,const uint8_t*art,uint8_t*dst,const R46PDRowJob*j){
 for(unsigned r=0;r<j->nruns;r++){const struct R46PDRun*p=&j->plan[r];unsigned e=map[r],at=(e&2047u)*32u;if(at+32>j->bytes)continue;unsigned ly=(e&0x1000u)?(j->ly^7u):j->ly;uint32_t bits=*(const pd_u32_alias*)(const void*)(art+at+ly*4);if(!bits)continue;uint32_t shifts=p->shifts;if(e&0x800)shifts^=0x39ce739c;r46_pd_nibble_span(bits,shifts,dst+p->x,p->n|(192u<<8));}
}
#endif
#if defined(__arm__)
__attribute__((target("thumb"),noinline,aligned(4)))
#endif
static void r46_pd_body_fast(void){
 R45PDVisual*s=r45_pd_visual();const uint16_t*m=r46_pd_body_map(s->body_id);
 unsigned y0=battle_compose_clip_active?battle_compose_clip_y0:0u,y1=battle_compose_clip_active?battle_compose_clip_y1:86u;if(y1>86u)y1=86u;
 for(unsigned y=y0;y<y1;y++){unsigned sy=r46_pd_ymap[y],start=(unsigned)(-s->fg[sy])&511u,phase=start&7u;
  const struct R46PDRun*plan=r46_pd_run_plan(phase);unsigned base=start>>3,n=r46_pd_nruns[phase];
  R46PDRowJob j={plan,base,sy&7u,s->body_bytes,0u};
  /* Plan columns increase consecutively from zero. The assembled body has
   * 40 columns inside a wrapping 64-column source plane; columns 40..63
   * are absent. Trim those runs before entering the ARM pixel kernel, while
   * retaining both visible intervals when a row crosses the source wrap. */
  if(base<40u){j.nruns=40u-base;if(j.nruns>n)j.nruns=n;r46_pd_body_row(m+(sy>>3)*40u+base,s->body_art,field_pause_fb+y*240u,&j);}
  if(base+n>64u){unsigned skip=64u-base;j.plan=plan+skip;j.nruns=n-skip;r46_pd_body_row(m+(sy>>3)*40u,s->body_art,field_pause_fb+y*240u,&j);}
 }
}
static void r45_pd_body(void){
 if(!battle_compose_clip_active||(battle_compose_clip_x0==0&&battle_compose_clip_x1==240)){r46_pd_body_fast();return;}
 R45PDVisual*s=r45_pd_visual();const uint16_t*m=r46_pd_body_map(s->body_id);
 /* PD is a fixed 40x15 assembled map with no death dissolve/resolved-source
  * indirection while its transformation controller is active. The generic
  * enemy span kernel rechecked all of those conditions for every 6-pixel run.
  * Consume the already-generated eight phase plans directly instead: same map,
  * flips, transparent nibble semantics and clip contract, but no shared-kernel
  * job setup/state copies or impossible branches. */
 for(unsigned y=0;y<86u;y++){
  if(battle_compose_clip_active&&(y<battle_compose_clip_y0||y>=battle_compose_clip_y1))continue;
  unsigned sy=r46_pd_ymap[y],ty=sy>>3,ly=sy&7u;if(ty>=15u)continue;
  unsigned start=(unsigned)(-s->fg[sy])&511u,phase=start&7u,base_tx=start>>3;
  uint8_t*dst=field_pause_fb+y*240u;const struct R46PDRun*plan=r46_pd_run_plan(phase);
  for(unsigned r=0;r<r46_pd_nruns[phase];r++){
   const struct R46PDRun*p=&plan[r];unsigned tx=(base_tx+p->tx)&63u;if(tx>=40u)continue;
   uint16_t e=m[ty*40u+tx];unsigned tile=e&0x07FFu;if(tile*32u+32u>s->body_bytes)continue;
   unsigned yy=(e&0x1000u)?7u-ly:ly;const uint8_t*rp=s->body_art+tile*32u+yy*4u;
   uint32_t bits=*(const pd_u32_alias*)(const void*)rp;if(!bits)continue;
   unsigned x=p->x,n=p->n,end=x+n;if(battle_compose_clip_active){unsigned cx0=(unsigned)(battle_compose_clip_x0<0?0:battle_compose_clip_x0),cx1=(unsigned)(battle_compose_clip_x1>240?240:battle_compose_clip_x1);if(x<cx0)x=cx0;if(end>cx1)end=cx1;if(end<=x)continue;n=end-x;}
   uint32_t shifts=p->shifts;if(e&0x0800u)shifts^=0x39CE739Cu;
   shifts>>=(x-p->x)*5u;r46_pd_nibble_span(bits,shifts,dst+x,n|(192u<<8));
  }
 }
}
static int r45_pd_background_compose(const struct BattleLiteActor*e){if(!r45_pd_formation(e))return 0;R45PDVisual*s=r45_pd_visual();if(!s->active)return 0;r45_pd_background();return 1;}
static int r45_pd_body_compose(const struct BattleLiteActor*e){if(!r45_pd_formation(e))return 0;R45PDVisual*s=r45_pd_visual();if(!s->active||!s->controller)return 0;r45_pd_body();return 1;}
/* The immutable source pool is the native equivalent of the decoded enemy
 * RAM bank. Do not stage it or a body at $4000: large PD banks overlap the
 * live Sound_Index bytes at $500A/$500B in the MD work mirror. */
static void r45_pd_source_init(unsigned id){R45PDVisual*s=r45_pd_visual();const uint32_t*n=battle_enemy_native_meta(id);s->source_bytes=(uint16_t)n[1];s->anim_id=(uint8_t)id;}
static void r45_pd_body_animate(unsigned id,unsigned source_bytes){R45PDVisual*s=r45_pd_visual();const uint32_t*nm=battle_enemy_native_meta(id);const uint8_t*source=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_SOURCE_OFF+nm[0];unsigned ops=battle_enemy_mapping_ops(id);for(unsigned op=0;op<ops;op++){
 unsigned table=0x10FCEu;uint32_t mp=(uint32_t)((int32_t)table+(int16_t)rom16(table+id*2u))+2u+op*6u;
 uint32_t d=battle_enemy_anim_descriptor(id,op);unsigned n=rom8(d)&127u,f=battle_enemy_anim_frame[0][op];if(!n||f>=n)f=0u;unsigned src=rom8(d+((rom8(d)&128u)?1u+n+f:2u+f))*32u,dst=rom8(mp)*32u,bytes=rom8(mp+1u)*32u;
 if(src+bytes<=source_bytes&&dst+bytes<=sizeof(s->body_art)){if(dst>s->body_bytes)dma_fill32(s->body_art+s->body_bytes,dst-s->body_bytes,0u);dma_copy32(source+src,s->body_art+dst,bytes);if(dst+bytes>s->body_bytes)s->body_bytes=(uint16_t)(dst+bytes);}
}}
static void r45_pd_body_snapshot(unsigned id){R45PDVisual*s=r45_pd_visual();const uint32_t*n=battle_enemy_native_meta(id);s->body_bytes=(uint16_t)(n[3]<=sizeof(s->body_art)?n[3]:0u);if(s->body_bytes){dma_copy32(ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_BASE_OFF+n[2],s->body_art,s->body_bytes);r45_pd_body_animate(id,n[1]);}}
static void r45_pd_queue(unsigned kind,unsigned id){R45PDVisual*s=r45_pd_visual();s->queue_kind=(uint8_t)kind;s->queue_id=(uint8_t)id;s->queue_at=0u;}
static void r45_pd_queue_tick(unsigned ram_queue){
 R45PDVisual*s=r45_pd_visual();unsigned kind=s->queue_kind,id=s->queue_id;if(!kind||((kind==3u)!=(ram_queue!=0u)))return;
 const uint32_t*n=battle_enemy_native_meta(id);const uint8_t*src=0;uint8_t*dst=0;unsigned bytes=0;
 if(kind==1u){src=ps4_precomp_assets+PS4_PRECOMP_BATTLE_ENEMY_NATIVE_BASE_OFF+n[2];dst=s->body_art;bytes=n[3];}
 else if(kind==2u){src=r45_pd_art_bank+r45_pd_bg_meta[id-0x85u][0];dst=s->bg_art;bytes=r45_pd_bg_meta[id-0x85u][1];}
 /* Retain the RAM-queue clock and byte count, without copying immutable
  * decoded data through live sound state. New sprites start after this queue. */
 else if(kind==3u){bytes=n[1];}
 else{bytes=id==0x86u?640u:3840u;}
 unsigned at=s->queue_at,take=bytes-at;if(take>128u)take=128u;if(dst&&take)dma_copy32(src+at,dst+at,take);s->queue_at=(uint16_t)(at+take);
 if(kind==1u&&at+take>s->body_bytes)s->body_bytes=(uint16_t)(at+take);
 if(kind==2u&&at+take>s->bg_bytes)s->bg_bytes=(uint16_t)(at+take);
 if(kind==3u&&at+take>s->source_bytes)s->source_bytes=(uint16_t)(at+take);
 if(s->queue_at==bytes){s->queue_kind=0u;if(kind==2u)s->bg_id=(uint8_t)(id-0x85u);}
}
static unsigned r45_pd_idle_tick(const struct BattleLiteActor*e){if(!r45_pd_formation(e))return 0u;R45PDVisual*s=r45_pd_visual();if(!s->active||s->controller)return 0u;r45_pd_palette_tick();r45_pd_palette_sync();s->bg_angle++;r46_pd_background_scroll(s->bg_angle);return 1u;}
