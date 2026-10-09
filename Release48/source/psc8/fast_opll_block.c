/* Block renderer for the PSC8 YM2413 synth (CP09).
 * Produces exactly the samples that n successive fastopll_calc() calls produce (same math,
 * reorganised channel-major so the per-sample work stays in registers). Register writes are
 * applied between blocks by the caller (catch-up rendering), so keys/frequencies are constant
 * inside a block. Verified bit-exact against fastopll_calc on the host (tests/fm_block_test.c) and on the
 * GBA (fmtest/).
 *
 * Per channel the block is cut into runs by the envelope state:
 *   - both envelopes constant, or one slowly changing (level constant for >= 16 samples): fm_steady_asm, levels fixed;
 *   - an envelope changing every few samples (attack / fast decay / release): fm_ramp_asm, which steps both envelopes and
 *     recomputes both levels inside the sample loop;
 *   - the single sample at a stage change (attack end, decay end, release end): plain C, as fastopll_calc.
 * The per-channel decode (phase increments, levels, rates) is cached in the synth state and refreshed only when one of the
 * registers it depends on is written. */
#include <stdint.h>
#include "fast_opll.h"
#ifndef HOT
#define HOT __attribute__((section(".hot")))
#endif
extern const int8_t fastopll_sine256[256];
extern const int8_t fastopll_halfsine256[256];
extern const uint8_t fastopll_patches[15][8];
extern const uint8_t fastopll_mul2[16];
extern const uint16_t fastopll_attack_step[16], fastopll_decay_step[16];

typedef struct { uint32_t pm, pc, inc_m, inc_c; int prev, modlev, carlev, fb; const int8_t *modtab, *cartab; } FmSteadyA;
/* X = (modulator envelope << 16) | carrier envelope, D = per-sample step of both in the same packing; tl / volf scale the levels */
typedef struct { uint32_t pm, pc, inc_m, inc_c; int prev; uint32_t X, D; int fb; const int8_t *modtab, *cartab; int tl, volf; } FmRampA;
void fm_steady_asm(FmSteadyA *s, int16_t *acc, unsigned n);
void fm_ramp_asm(FmRampA *s, int16_t *acc, unsigned n);
#ifdef FM_STEADY_C
/* C mirrors of the two assembly loops (host tests) */
void fm_steady_asm(FmSteadyA *s, int16_t *acc, unsigned n){
  uint32_t pm=s->pm, pc=s->pc, inc_m=s->inc_m, inc_c=s->inc_c; int prev=s->prev, modlev=s->modlev, carlev=s->carlev, fb=s->fb;
  const int8_t *mt=s->modtab, *ct=s->cartab;
  for(unsigned i=0;i<n;i++){
    pm+=inc_m; int mi=(int)(pm>>24)+((prev*fb)>>6);
    int m=(mt[mi&255]*modlev)>>14; prev=m;
    pc+=inc_c; int c=ct[((int)(pc>>24)+(m>>2))&255];
    acc[i]=(int16_t)(acc[i]+((c*carlev)>>12));
  }
  s->pm=pm; s->pc=pc; s->prev=prev;
}
void fm_ramp_asm(FmRampA *s, int16_t *acc, unsigned n){
  uint32_t pm=s->pm, pc=s->pc, inc_m=s->inc_m, inc_c=s->inc_c, X=s->X, D=s->D; int prev=s->prev, fb=s->fb, tl=s->tl, volf=s->volf;
  const int8_t *mt=s->modtab, *ct=s->cartab;
  for(unsigned i=0;i<n;i++){
    X+=D;
    pm+=inc_m; int mi=(int)(pm>>24)+((prev*fb)>>6);
    int m=((mt[mi&255]*(int)(X>>24))*tl)>>14; prev=m;
    pc+=inc_c; int c=ct[((int)(pc>>24)+(m>>2))&255];
    acc[i]=(int16_t)(acc[i]+(((c*(int)((X>>8)&255u))*volf)>>12));
  }
  s->pm=pm; s->pc=pc; s->prev=prev;
}
#endif

#ifdef FM_STATS            /* host path statistics: [0] steady calls [1] steady samples [2] ramp calls [3] ramp samples [4] single samples [5] channel blocks */
extern unsigned long fm_stat[8];
#define FMS(k,v) (fm_stat[k]+=(v))
#else
#define FMS(k,v) ((void)0)
#endif
/* ---- envelope runs ----------------------------------------------------------------------------------------------------
 * For the next updates of one envelope (value v, 16-bit):
 *   lin  = number of upcoming updates that are plain linear steps of `step` (no stage change, no clamp); ENV_INF if it never moves
 *   run  = number of upcoming updates after which (value>>8) is still what it is after the first one (constant level), only for
 *          steps < 64 where that is worth chunking; 0 otherwise
 *   v1   = value after the first update
 * lin == 0 means the very next update is a stage change (handled sample by sample). */
#define ENV_INF 0x7fffffffu
typedef struct { uint32_t run, lin, v1; int32_t step; } EnvR;
/* ceil(2^32/d) for the attack / decay step tables (index 0, 1 and 2 of decay are 0 / d==1 handled separately) */
static const uint32_t atk_rcp[16]={0,536870912u,357913942u,268435456u,178956971u,134217728u,89478486u,67108864u,44739243u,33554432u,22369622u,16777216u,11184811u,5592406u,2097152u,524288u};
static const uint32_t dec_rcp[16]={0,0,0,2147483648u,2147483648u,1431655766u,1073741824u,715827883u,536870912u,357913942u,268435456u,178956971u,134217728u,89478486u,44739243u,22369622u};
static inline uint32_t divr(uint32_t x,uint32_t d,uint32_t rcp){ /* floor(x/d) for x < 65536 */
  return d==1?x:(uint32_t)(((uint64_t)x*rcp)>>32); }
__attribute__((section(".hot"),noinline)) static void env_calc(EnvR *e,uint32_t key,uint32_t st,uint32_t v,uint32_t ai,uint32_t di,uint32_t sus,uint32_t ri){
  uint32_t d,j2; e->run=0; e->lin=0; e->step=0; e->v1=v;
  if(key){
    if(st==0){
      if(ai==15) return;                                         /* instant attack: the next update is the stage change */
      if(ai==0){ if(v<65535) e->run=e->lin=ENV_INF; return; }
      d=fastopll_attack_step[ai]; if(v+d>=65535) return;
      e->lin=divr(65534u-v,d,atk_rcp[ai]); e->step=(int32_t)d; e->v1=v+d;
      if(d<64){ j2=divr(((v+d)|255u)-v,d,atk_rcp[ai]); e->run=e->lin<j2?e->lin:j2; }
      return;
    }
    if(st==1){
      if(di==0){ if(v>sus) e->run=e->lin=ENV_INF; return; }
      d=fastopll_decay_step[di]; if(v<=sus+d) return;
      e->lin=divr(v-sus-1u,d,dec_rcp[di]); e->step=-(int32_t)d; e->v1=v-d;
      if(d<64){ j2=divr(v-((v-d)&~255u),d,dec_rcp[di]); e->run=e->lin<j2?e->lin:j2; }
      return;
    }
    e->run=e->lin=ENV_INF; return;                               /* sustain: constant */
  }
  if(!v){ e->run=e->lin=ENV_INF; return; }
  d=fastopll_decay_step[ri]; if(v<=d) return;
  e->lin=divr(v-1u,d,dec_rcp[ri]); e->step=-(int32_t)d; e->v1=v-d;
  if(d<64){ j2=divr(v-((v-d)&~255u),d,dec_rcp[ri]); e->run=e->lin<j2?e->lin:j2; }
}

static void chan_decode(FastOPLL *o,unsigned ch){
  FmChP *c=&o->chp[ch];
  uint8_t r20=o->reg[0x20+ch]; uint32_t f=(uint32_t)o->reg[0x10+ch]|((uint32_t)(r20&1)<<8), block=(r20>>1)&7;
  uint8_t iv=o->reg[0x30+ch],inst=iv>>4,vol=iv&15; const uint8_t *p=inst?fastopll_patches[(inst-1)%15]:o->reg;
  uint32_t base=(f<<block)*22430u;
  c->nz=f!=0; c->inc_m=(base*fastopll_mul2[p[0]&15])>>1; c->inc_c=(base*fastopll_mul2[p[1]&15])>>1;
  c->tl=63-(p[2]&63); c->fb=p[3]&7; c->volf=15-vol; c->halfm=(p[3]>>3)&1; c->halfc=(p[3]>>4)&1;
  c->mar_i=p[4]>>4; c->mdr_i=p[4]&15; c->mrr_i=(p[6]&15)?(p[6]&15):5;
  c->car_i=p[5]>>4; c->cdr_i=p[5]&15; c->crr_i=(p[7]&15)?(p[7]&15):5;
  c->msus=(uint16_t)(((15u-(p[6]>>4))*65535u)/15u); c->csus=(uint16_t)(((15u-(p[7]>>4))*65535u)/15u);
}

/* acc receives the raw channel sum; the YM2413 output sample is clamp(acc>>3, -128, 127) (applied by the mixer). */
HOT void fastopll_render(FastOPLL *o, int16_t *acc, unsigned n){
  uint8_t rhythm=o->reg[0x0e]&0x20; unsigned channels=rhythm?6:9;
  for(unsigned i=0;i<n;i++) acc[i]=0;
  for(unsigned ch=0;ch<channels;ch++){
    uint32_t key=(o->reg[0x20+ch]>>4)&1;
    uint32_t env=o->env[ch],menv=o->mod_env[ch]; uint32_t st=o->stage[ch],mst=o->mod_stage[ch],old=o->key_prev[ch];
    if(key&&!old){env=menv=0;st=mst=0;} if(!key&&old){st=mst=3;} o->key_prev[ch]=(uint8_t)key;
    if(o->cdirty&(1u<<ch)){ chan_decode(o,ch); o->cdirty&=(uint16_t)~(1u<<ch); }
    const FmChP *cp=&o->chp[ch];
    if(!cp->nz||(!key&&!env&&!menv)){o->env[ch]=(uint16_t)env;o->mod_env[ch]=(uint16_t)menv;o->stage[ch]=(uint8_t)st;o->mod_stage[ch]=(uint8_t)mst;continue;}
    FMS(5,1);
    uint32_t pm=o->pm[ch], pc=o->pc[ch]; int prev=o->prevm[ch];
    uint32_t inc_m=cp->inc_m, inc_c=cp->inc_c; int tl=cp->tl, fb=cp->fb, volf=cp->volf;
    const int8_t *mtab=cp->halfm?fastopll_halfsine256:fastopll_sine256, *ctab=cp->halfc?fastopll_halfsine256:fastopll_sine256;
    unsigned i=0;
    while(i<n){
      if(!key&&!env&&!menv) break;                         /* channel goes idle: state frozen, as fastopll_calc */
      unsigned rem=n-i;
      EnvR em,ec;
      env_calc(&em,key,mst,menv,cp->mar_i,cp->mdr_i,cp->msus,cp->mrr_i);
      env_calc(&ec,key,st,env,cp->car_i,cp->cdr_i,cp->csus,cp->crr_i);
      uint32_t r=em.run<ec.run?em.run:ec.run; if(r>rem) r=rem;
      if(r>=16 || (em.run==ENV_INF&&ec.run==ENV_INF)){
        /* constant levels for the whole run */
        FmSteadyA fa={pm,pc,inc_m,inc_c,prev,tl*(int)(em.v1>>8),(int)(ec.v1>>8)*volf,fb,mtab,ctab};
        FMS(0,1); FMS(1,r);
        fm_steady_asm(&fa,acc+i,r); pm=fa.pm; pc=fa.pc; prev=fa.prev;
        menv=(uint32_t)((int32_t)menv+em.step*(int32_t)r); env=(uint32_t)((int32_t)env+ec.step*(int32_t)r);
        i+=r; continue;
      }
      uint32_t rl=em.lin<ec.lin?em.lin:ec.lin; if(rl>rem) rl=rem;
      if(rl>=1){
        /* envelopes step linearly through the whole run: levels follow them sample by sample */
        FmRampA ra={pm,pc,inc_m,inc_c,prev,(menv<<16)|env,(uint32_t)(em.step*65536+ec.step),fb,mtab,ctab,tl,volf};
        FMS(2,1); FMS(3,rl);
        fm_ramp_asm(&ra,acc+i,rl); pm=ra.pm; pc=ra.pc; prev=ra.prev;
        menv=(uint32_t)((int32_t)menv+em.step*(int32_t)rl); env=(uint32_t)((int32_t)env+ec.step*(int32_t)rl);
        i+=rl; continue;
      }
      FMS(4,1);
      { /* one sample at a stage change (or anything the run logic does not cover) */
        uint32_t mar=fastopll_attack_step[cp->mar_i], mdr=fastopll_decay_step[cp->mdr_i], mrr=fastopll_decay_step[cp->mrr_i];
        uint32_t car=fastopll_attack_step[cp->car_i], cdr=fastopll_decay_step[cp->cdr_i], crr=fastopll_decay_step[cp->crr_i];
        uint32_t msus=cp->msus, csus=cp->csus;
        /* modulator envelope */
        if(key){
          if(mst==0){uint32_t e=menv+mar;if(cp->mar_i==15||e>=65535){menv=65535;mst=1;}else menv=e;}
          else if(mst==1){if(menv>msus+mdr)menv-=mdr;else{menv=msus;mst=2;}}
        }else if(menv){menv=menv>mrr?menv-mrr:0;}
        /* carrier envelope */
        if(key){
          if(st==0){uint32_t e=env+car;if(cp->car_i==15||e>=65535){env=65535;st=1;}else env=e;}
          else if(st==1){if(env>csus+cdr)env-=cdr;else{env=csus;st=2;}}
        }else if(env){env=env>crr?env-crr:0;}
        pm+=inc_m; pc+=inc_c;
        int mi=(int)(pm>>24); if(fb) mi+=(prev*fb)>>6;
        int m=mtab[mi&255]; m=(m*tl*(int)(menv>>8))>>14; prev=m;
        int ci=(int)(pc>>24)+(m>>2); int c=ctab[ci&255];
        acc[i]=(int16_t)(acc[i]+((c*(int)(env>>8)*volf)>>12));
        i++;
      }
    }
    o->pm[ch]=pm;o->pc[ch]=pc;o->env[ch]=(uint16_t)env;o->mod_env[ch]=(uint16_t)menv;o->stage[ch]=(uint8_t)st;o->mod_stage[ch]=(uint8_t)mst;o->prevm[ch]=(int16_t)prev;
  }
  if(rhythm) fastopll_rhythm(o,acc,n);
}
