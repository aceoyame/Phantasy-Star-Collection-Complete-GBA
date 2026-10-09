/* Dispatcher for statically recompiled 68000 code (CP09).
 * m68k_exec(limit) keeps the interpreter's contract: run instructions until C.cycles >= limit (at least one),
 * returning early after STOP or an interrupt-mask/supervisor change.  Translated regions run natively; anything
 * else runs in the interpreter, which hands control back as soon as it branches into translated code; single
 * unsupported instructions use the reference step.
 * RAM code (0xFF0000-0xFF7FFF, copied by the game from cartridge 0x20000) is only run natively while its bytes
 * still equal the cartridge copy the translation was made from: every RAM write in that window bumps a per-256-byte
 * generation (aot_rgen); a region is re-compared with the cartridge when the generations it spans changed. */
#include "aotrt.h"

u32 aot_lim;
u32 aot_rgen[128];
u32 aot_code_epoch HOTDATA = 1u;
u32 aot_ncalls, aot_ninterp;
static const AotGame* G;
#define MAXRR 64u
static u32 rchecked_epoch[MAXRR];
static u32 rstate[MAXRR];                    /* RAM region: generation sum at the last check (+1), bit31 = bytes differ */
#define CACHE 512u
static struct { u32 pc; aot_fn fn; u16 idx, rr; u32 epoch; } cache[CACHE];

/* On epoch rollover invalidate all timestamps, so ancient checks cannot alias. */
__attribute__((section(".text.rom"),noinline)) void aot_epoch_wrap(void){
 aot_code_epoch=1u;
 for(u32 i=0;i<CACHE;i++)cache[i].epoch=0u;
 for(u32 i=0;i<MAXRR;i++){rchecked_epoch[i]=0u;rstate[i]=0u;}
}
__attribute__((section(".text.rom"))) void aot_select_index(u32 index){
  extern const AotGame aot_games[]; extern const u32 aot_ngames;
  G=0; for(u32 i=0;i<aot_ngames;i++) if(aot_games[i].index==index) G=&aot_games[i];
  if(G&&G->nrr>MAXRR)G=0;
  for(u32 i=0;i<CACHE;i++) cache[i].pc=0xFFFFFFFFu;
  aot_code_epoch=1u;
  for(u32 i=0;i<MAXRR;i++){rstate[i]=0;rchecked_epoch[i]=0;}
  for(u32 i=0;i<128;i++) aot_rgen[i]=0;
}
extern const u32 aot_pool[][64]; extern const aot_fn aot_fns[]; extern const AotGame aot_games[]; extern const u32 aot_ngames;
int aot_has(u32 pc){
  pc&=0xFFFFFFu; if(!G) return 0;
  u32 p;
  if(pc<0x40000u) p=G->rompg[pc>>12];
  else if(pc-0xFF0000u<0x8000u){ pc-=0xFF0000u; p=G->rampg[pc>>12]; }
  else return 0;
  if(p==0xFFFFu) return 0;
  return (aot_pool[p][(pc&0xFFFu)>>6]>>((pc>>1)&31u))&1u;
}
/* RAM region still identical to the cartridge copy? */
__attribute__((section(".text.rom"),noinline)) static int ram_ok(u32 rr){
  if(rchecked_epoch[rr]==aot_code_epoch)return !(rstate[rr]>>31);
  rchecked_epoch[rr]=aot_code_epoch;
  const AotRamReg*r=&G->rr[rr]; const u16*q=G->rng+r->first*2u; u32 sum=1;
  for(u32 k=0;k<r->n;k++){ u32 o=q[2*k],l=q[2*k+1]; for(u32 b=o>>8;b<=((o+l-1u)>>8);b++) sum+=aot_rgen[b]; }
  sum&=0x7FFFFFFFu;
  u32 st=rstate[rr]; if((st&0x7FFFFFFFu)==sum) return !(st>>31);
  u32 ok=1;
  for(u32 k=0;k<r->n&&ok;k++){ u32 o=q[2*k],l=q[2*k+1];
    for(u32 j=0;j<l;j++){ u32 a=0x20000u+o+j; if(a>=m68k_romsize||((const u8*)md_ram)[(o+j)^1u]!=m68k_romp[a>>12][a&0xFFFu]){ ok=0; break; } } }
  rstate[rr]=sum|(ok?0u:0x80000000u); return (int)ok;
}
static aot_fn lookup(u32 pc,u32*idx){
  u32 h=((pc>>1)^(pc>>9))&(CACHE-1u); u32 rr;
  if(cache[h].pc==pc){
    *idx=cache[h].idx; rr=cache[h].rr;
    if(rr==0xFFFFu||cache[h].epoch==aot_code_epoch)return cache[h].fn;
    if(ram_ok(rr)){cache[h].epoch=aot_code_epoch;return cache[h].fn;}return 0;
  }
  if(!aot_has(pc)) return 0;
  const AotEnt*t; u32 n;
  if(pc<0x40000u){ t=G->rom; n=G->nrom; } else { t=G->ram; n=G->nram; }
  u32 lo=0,hi=n;
  while(lo<hi){ u32 m=(lo+hi)>>1; if(t[m].pc<pc) lo=m+1; else hi=m; }
  if(lo>=n||t[lo].pc!=pc){
    if(pc>=0x40000u) return 0;
    t=G->xrom; n=G->nxrom; lo=0; hi=n;                 /* instructions whose bytes differ between games: this game's own table */
    while(lo<hi){ u32 m=(lo+hi)>>1; if(t[m].pc<pc) lo=m+1; else hi=m; }
    if(lo>=n||t[lo].pc!=pc) return 0;
  }
  const AotEnt*e=&t[lo]; aot_fn f=aot_fns[e->fn];
  cache[h].pc=pc; cache[h].fn=f; cache[h].idx=e->idx; cache[h].rr=e->rr;
  *idx=e->idx;cache[h].epoch=0u;
  if(e->rr!=0xFFFFu){if(!ram_ok(e->rr))return 0;cache[h].epoch=aot_code_epoch;}
  return f;
}

void m68k_exec_interp(u32 limit);
void m68k_exec(u32 limit){
  aot_lim=limit; C.idle_ok=0;
  if(!G){ m68k_exec_interp(limit); return; }
  do{
    u32 pc=C.pc&0xFFFFFFu, idx; aot_fn fn=lookup(pc,&idx);
    if(fn){
      C.spin_valid=0; C.spin_tgt=0xFFFFFFFFu; aot_ncalls++;
      u32 rc=fn(idx);
      /* Native SR-changing returns preserve the reference instruction boundary. */
      if(rc==2u)break;
      if(rc==AOT_INTERP){ u32 old=C.sr; ref_step(); if(C.stopped||((old^C.sr)&0x2700u)) break; }
      continue;
    }
    if(pc>=0xE00000u&&md_ram[(pc&0xFFFFu)>>1]==0x4EF9u&&(pc&0xFFFFu)<0xFFFAu){   /* RAM jump-table entry: JMP abs.l */
      C.cycles+=12u; C.pc=((u32)md_ram[((pc+2u)&0xFFFFu)>>1]<<16)|md_ram[((pc+4u)&0xFFFFu)>>1]; continue;
    }
    { u32 idle=C.idle_ok; aot_ninterp++;
#ifdef MD_HOST
      { extern u32 aot_miss[1<<24]; u32 c0=C.cycles; m68k_exec_interp(limit); aot_miss[pc]+=C.cycles-c0; }
#else
      m68k_exec_interp(limit);
#endif
      if(!C.idle_ok) C.idle_ok=idle;
      if(C.stopped) break;
      if(!aot_has(C.pc)&&!((C.pc&0xFFFFFFu)>=0xE00000u&&md_ram[(C.pc&0xFFFFu)>>1]==0x4EF9u)) break;
    }
  }while((s32)(limit-C.cycles)>0);
}
