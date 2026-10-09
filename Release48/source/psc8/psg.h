/* Sega integrated SN76489. Fixed GBA Timer0 /924 output clock.
 * Tones/noise are integrated across each sample, rather than point sampled.
 * This is a box filter, not an analog model or a sinc resampler.
 */
#ifndef PSC8_PSG_H
#define PSC8_PSG_H
#include <stdint.h>
#ifndef HOT
#define HOT
#endif
static uint16_t tones[3],noise_lfsr;
static uint8_t volumes[4],noise,psg_latch,psg_clock_fraction;
static uint32_t phases[4];
static const uint16_t amplitudes[16] __attribute__((section(".hotdata.psgtab")))={1200,953,757,601,477,379,301,239,190,151,120,95,75,60,48,0};
HOT static void psg_write(uint8_t v){
 if(v&128)psg_latch=(v>>4)&7;
 unsigned c=psg_latch>>1;
 if(psg_latch&1)volumes[c]=v&15;
 else if(c<3){if(v&128)tones[c]=(tones[c]&0x3f0)|(v&15);else tones[c]=(tones[c]&15)|((v&63)<<4);}
 else {noise=v&7;noise_lfsr=0x8000;phases[3]=0;}
}
HOT static uint32_t psg_step(void){
 /* 3579545 * 924 * 65536 / 16777216 = 12919920 + 60/256. */
 unsigned f=psg_clock_fraction+60u;psg_clock_fraction=(uint8_t)f;
 return 12919920u+(f>>8);
}
HOT static int psg_tone(unsigned c,uint32_t step){
 unsigned period=tones[c];if(period<=1)return amplitudes[volumes[c]];
 uint32_t limit=period<<21,half=limit>>1,p=phases[c];
 while(p>=limit)p-=limit;
 int start=(int)(p<=half?p:limit-p);
 p+=step;while(p>=limit)p-=limit;phases[c]=p;
 int end=(int)(p<=half?p:limit-p);
 /* Scale before multiplication to keep every operation within signed 32 bits.
  * The Q8 integration error is <0.02 output units at maximum volume. */
 return ((end-start)/256*(int)amplitudes[volumes[c]])/50468;
}
HOT static int psg_noise(uint32_t step){
 unsigned period=(noise&3)==3?tones[2]:(16u<<(noise&3));if(!period)period=1;
 uint32_t limit=period<<21,p=phases[3];
 /* A tone-2 frequency write takes effect immediately; normalize its divider. */
 while(p>=limit)p-=limit;
 int area=0;uint32_t left=step;
 while(left){
  uint32_t span=limit-p;if(span>left)span=left;
  area+=(noise_lfsr&1)?(int)span:-(int)span;
  p+=span;left-=span;
  if(p==limit){p=0;unsigned bit=(noise&4)?((noise_lfsr^(noise_lfsr>>3))&1):(noise_lfsr&1);noise_lfsr=(noise_lfsr>>1)|(bit<<15);}
 }
 phases[3]=p;
 return (area/256*(int)amplitudes[volumes[3]])/50468;
}
#ifdef PSG_TONE_ASM
typedef struct { uint32_t p, limit, half, tri, amp, fr; } PsgToneA;
void psg_tone_asm(PsgToneA *t, int16_t *L, int16_t *R, unsigned n);
static int16_t psg_dummy[320];
#endif
/* Closed-form phase advance for channels whose output is not needed (muted / not routed) over n >= PSG_SKIP_MIN samples.
 * The per-sample loops add 12919920 + carry per sample (carry pattern of the 60/256 clock fraction); adding it n times and
 * reducing modulo the limit gives exactly the same phase (and, for noise, the same number of LFSR clocks). */
#define PSG_SKIP_MIN 12u
static inline uint64_t psg_total(unsigned n,uint8_t fr0){return (uint64_t)n*12919920u+(((unsigned)fr0+60u*n)>>8);}
static inline uint32_t psg_divmod(uint32_t x,uint32_t d,uint32_t *q){   /* x < 4096, 1 <= d < 1024 */
 uint32_t qq=0;for(int k=11;k>=0;k--){uint32_t dk=d<<k;if(dk<=x){x-=dk;qq|=1u<<k;}}*q=qq;return x;}
/* Cold, ROM-resident helpers (one call replaces n per-sample iterations; IWRAM is full). */
__attribute__((noinline)) static uint32_t psg_skip_tone(uint32_t p,unsigned period,unsigned n,uint8_t fr){
 if(n<PSG_SKIP_MIN){uint32_t limit=period<<21;for(unsigned i=0;i<n;i++){unsigned f=fr+60u;fr=(uint8_t)f;p+=12919920u+(f>>8);while(p>=limit)p-=limit;}return p;}
 uint64_t x=(uint64_t)p+psg_total(n,fr);uint32_t q;uint32_t r=psg_divmod((uint32_t)(x>>21),period,&q);
 return (r<<21)|((uint32_t)x&0x1fffffu);}
__attribute__((noinline)) static void psg_skip_noise(unsigned n,uint8_t fr){
 unsigned period=(noise&3)==3?tones[2]:(16u<<(noise&3));if(!period)period=1;
 uint32_t limit=period<<21,p=phases[3];while(p>=limit)p-=limit;
 uint64_t x=(uint64_t)p+psg_total(n,fr);uint32_t q;uint32_t r=psg_divmod((uint32_t)(x>>21),period,&q);
 uint32_t l=noise_lfsr;
 if(noise&4){for(;q;q--)l=(l>>1)|(((l^(l>>3))&1u)<<15);}else{for(;q;q--)l=(l>>1)|((l&1u)<<15);}
 noise_lfsr=(uint16_t)l;phases[3]=(r<<21)|((uint32_t)x&0x1fffffu);}
/* Block form of n successive (psg_step, psg_tone x3, psg_noise) evaluations, adding the routed
 * channel outputs into L/R. Bit-exact with the per-sample path. */
HOT static void psg_render(int16_t *L,int16_t *R,unsigned n,unsigned stereo_mask,unsigned enabled){
 uint8_t frac0=psg_clock_fraction;
 for(unsigned c=0;c<4;c++){
  unsigned toL=enabled&&(stereo_mask&(16u<<c)),toR=enabled&&(stereo_mask&(1u<<c));
  uint8_t fr=frac0;
  if(c<3){
   unsigned period=tones[c];int amp=amplitudes[volumes[c]];
   if(period<=1){ if(toL)for(unsigned i=0;i<n;i++)L[i]=(int16_t)(L[i]+amp); if(toR)for(unsigned i=0;i<n;i++)R[i]=(int16_t)(R[i]+amp); continue; }
   uint32_t limit=period<<21,half=limit>>1,p=phases[c];
   while(p>=limit)p-=limit;
   if(!(toL|toR)||!amp){                     /* inaudible: only the phase moves */
    phases[c]=psg_skip_tone(p,period,n,fr);continue;
   }
   int tri=(int)(p<=half?p:limit-p);
#ifdef PSG_TONE_ASM
   { PsgToneA t={p,limit,half,(uint32_t)tri,(uint32_t)amp,fr}; psg_tone_asm(&t,toL?L:psg_dummy,toR?R:psg_dummy,n); p=t.p; }
#else
   for(unsigned i=0;i<n;i++){
    unsigned f=fr+60u;fr=(uint8_t)f;
    p+=12919920u+(f>>8);while(p>=limit)p-=limit;
    int end=(int)(p<=half?p:limit-p);int s=((end-tri)/256*amp)/50468;tri=end;
    if(toL)L[i]=(int16_t)(L[i]+s);if(toR)R[i]=(int16_t)(R[i]+s);
   }
#endif
   phases[c]=p;
  }else{
   if(n>=PSG_SKIP_MIN&&(!(toL|toR)||!amplitudes[volumes[3]])){      /* inaudible: advance the shift register in closed form */
    psg_skip_noise(n,fr);
   }else
   for(unsigned i=0;i<n;i++){
    unsigned f=fr+60u;fr=(uint8_t)f;uint32_t step=12919920u+(f>>8);
    int s=psg_noise(step);
    if(toL)L[i]=(int16_t)(L[i]+s);if(toR)R[i]=(int16_t)(R[i]+s);
   }
  }
 }
 /* advance the shared clock fraction by n samples */
 psg_clock_fraction=(uint8_t)(frac0+60u*n);
}
HOT static int psg_enabled(unsigned present,unsigned control){return !present || control==0 || control==3;}
#endif
