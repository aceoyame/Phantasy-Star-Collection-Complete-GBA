/* Inner loops of the Genesis sound mixer (ARM, IWRAM on the GBA: listed in HOT_FILES).
 * acc[] holds output * 65536; a table sample is a signed byte, the weight carries amplitude * stage gain * level. */
#include "md.h"
#ifdef MD_HOST
unsigned long long snd_cnt[4];
#define CNT(k,n) (snd_cnt[k]+=(n))
#else
#define CNT(k,n) ((void)0)
#endif
static inline s32 sample_mac(s32 acc,s32 sample,s32 weight) {
#ifdef MD_HOST
  return acc+sample*weight;
#else
  /* ARM7 early termination uses Rs: keep the signed byte there. */
  __asm__("mla %0,%1,%2,%0" : "+&r"(acc) : "r"(weight),"r"(sample));
  return acc;
#endif
}

/* one table (or a sine), phase `ph` (32 bit = one table cycle), n samples */
void snd_mix1(s32 *acc, u32 n, u32 *pph, u32 step, const s8 *tab, s32 w, u32 sh){
  u32 ph=*pph; CNT(0,n);
  for(u32 i=0;i<n;i++){ acc[i]=sample_mac(acc[i],tab[ph>>sh],w); ph+=step; }
  *pph=ph;
}

/* two tables (each its own length) read at the same phase and weighted separately: a timbre cross-fade */
#ifdef MD_HOST
void snd_mix2(s32 *acc, u32 n, u32 *pph, u32 step, const s8 *ta, const s8 *tb, s32 wa, s32 wb, u32 sha, u32 shb){
  u32 ph=*pph; CNT(1,n);
  for(u32 i=0;i<n;i++){ acc[i]=sample_mac(sample_mac(acc[i],ta[ph>>sha],wa),tb[ph>>shb],wb); ph+=step; }
  *pph=ph;
}

#else
/* CP55: paired exact ARM7 samples. An odd head shares the second body;
 * one LDM reads all six stack arguments, and signed bytes remain MLA Rs. */
__attribute__((naked)) void snd_mix2(s32 *acc, u32 n, u32 *pph, u32 step, const s8 *ta, const s8 *tb, s32 wa, s32 wb, u32 sha, u32 shb){
 __asm__ volatile("ldr r12,[r2]\n"
"cmp r1,#0\n"
"beq 4f\n"
"push {r4-r10,lr}\n"
"add r5,sp,#32\n"
"ldmia r5,{r4,r7-r10,lr}\n"
"movs r1,r1,lsr #1\n"
"adc r1,r1,#0\n"
"bcs 2f\n"
"1:\n"
"add r5,r4,r12,lsr r10\n"
"ldr r6,[r0]\n"
"ldrsb r5,[r5]\n"
"mla r6,r8,r5,r6\n"
"add r5,r7,r12,lsr lr\n"
"add r12,r12,r3\n"
"ldrsb r5,[r5]\n"
"mla r6,r9,r5,r6\n"
"str r6,[r0],#4\n"
"2:\n"
"add r5,r4,r12,lsr r10\n"
"ldr r6,[r0]\n"
"ldrsb r5,[r5]\n"
"mla r6,r8,r5,r6\n"
"add r5,r7,r12,lsr lr\n"
"add r12,r12,r3\n"
"ldrsb r5,[r5]\n"
"mla r6,r9,r5,r6\n"
"str r6,[r0],#4\n"
"subs r1,r1,#1\n"
"bne 1b\n"
"pop {r4-r10,lr}\n"
"4:\n"
"str r12,[r2]\n"
"bx lr\n"
);
}
#endif

/* noise: xorshift32, uniform bytes; the weight is for an rms of 127 (uniform bytes have 74, hence the 1.75) */
void snd_mixn(s32 *acc, u32 n, u32 *plfsr, s32 w){
  u32 x=*plfsr; w=(w*7)>>2; CNT(2,n);
  for(u32 i=0;i<n;i++){ x^=x<<13; x^=x>>17; x^=x<<5; acc[i]=sample_mac(acc[i],(s8)(x>>24),w); }
  *plfsr=x;
}

/* The sole runtime caller uses Q8 gain 256.  ((x>>8)*256)>>16 is
 * exactly x>>16, including negative values; no per-sample multiply is needed. */
void snd_out256(const s32 *m, const s32 *l, const s32 *r, s8 *oa, s8 *ob, u32 n){
  for(u32 i=0;i<n;i++){
    s32 a=(m[i]+l[i])>>16, b=(m[i]+r[i])>>16;
#ifndef MD_HOST
    /* ARM7 branchless signed saturation; preserves the exact Q16 PCM result. */
    __asm__ volatile("cmn %0,#128\n\tmvnlt %0,#127\n\tcmp %0,#127\n\tmovgt %0,#127" : "+r"(a) :: "cc");
    __asm__ volatile("cmn %0,#128\n\tmvnlt %0,#127\n\tcmp %0,#127\n\tmovgt %0,#127" : "+r"(b) :: "cc");
    oa[i]=(s8)a; ob[i]=(s8)b;
#else
    oa[i]=(s8)(a>127?127:a<-128?-128:a);
    ob[i]=(s8)(b>127?127:b<-128?-128:b);
#endif
  }
}

/* CP50: equal L/R output when all active YM channels are center/muted. */
void snd_outmono(const s32 *m, s8 *oa, s8 *ob, u32 n){
  for(u32 i=0;i<n;i++){
    s32 a=m[i]>>16;
#ifndef MD_HOST
    __asm__ volatile("cmn %0,#128\n\tmvnlt %0,#127\n\tcmp %0,#127\n\tmovgt %0,#127" : "+r"(a) :: "cc");
#else
    a=a>127?127:a<-128?-128:a;
#endif
    oa[i]=(s8)a; ob[i]=(s8)a;
  }
}
