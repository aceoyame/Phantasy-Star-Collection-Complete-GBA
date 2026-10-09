/* Host transport simulator for gba/ps4_audio_stream.c (R13 code, unmodified
 * logic; only REG16/REG32 redirected to arrays).  Models:
 *   - VBlank edges every 528 samples (1 sample = 532 cycles, exact frame)
 *   - PSIV main loop: work W (samples) -> waitvb(): prepare at entry, then
 *     wait for the first VBlank edge after entry, then commit
 *   - DMA1/FIFO A: after a commit that writes DMA1CNT_H=0xB640, sample k
 *     of the new stream is read from DMA1SAD+k; with no new commit the DMA
 *     keeps incrementing (repeat FIFO DMA has no end) -- exactly the hardware.
 * Reports: bytes read outside mix_buffer (garbage run-on), played frames that
 * are exact next-in-order / repeats / other (splice/garbage), and music tempo
 * (source frames rendered per VBlank). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ps4_audio_runtime.h"
volatile uint16_t hw_reg16[0x200]; volatile uint32_t hw_reg32[0x100];
const int8_t* host_mix_buffer(void); unsigned host_mix_buffer_size(void);
extern unsigned prof_fm, prof_psg, prof_noise, prof_dac;
#define SPF 528u
#define MAXF 20000
static int8_t rlog[MAXF][224]; static unsigned rn;
static unsigned pf[MAXF],pp[MAXF],pn[MAXF],pd[MAXF];
void __real_ps4_audio_fallback_render_224(int8_t*);
void __wrap_ps4_audio_fallback_render_224(int8_t* o){
  unsigned f0=prof_fm,p0=prof_psg,n0=prof_noise,d0=prof_dac;
  __real_ps4_audio_fallback_render_224(o);
  if(rn<MAXF){memcpy(rlog[rn],o,224);pf[rn]=prof_fm-f0;pp[rn]=prof_psg-p0;pn[rn]=prof_noise-n0;pd[rn]=prof_dac-d0;} rn++;
}
static uint64_t t;                 /* sample clock */
static const int8_t* src; static unsigned long off; static int dma_on;
static long garbage, inguard_stale; static int8_t* out; static unsigned long on;
static void settm3(void){ hw_reg16[0x10C>>1]=(uint16_t)((t*532u)/64u); }
static void play_until(uint64_t end){
  const int8_t* mb=host_mix_buffer(); unsigned mbs=host_mix_buffer_size();
  while(t<end){ int8_t v=0;
    if(dma_on&&src){ const int8_t* p=src+off; if(p<mb||p>=mb+mbs){garbage++; v=(int8_t)0x55;} else v=*p; off++; }
    out[on++]=v; t++; }
}
static void commit(void){ settm3(); hw_reg16[0xC6>>1]=hw_reg16[0xC6>>1]; uint16_t before=hw_reg16[0xC6>>1];
  hw_reg16[0xC6>>1]=0xFFFF; ps4_audio_vblank_commit();
  if(hw_reg16[0xC6>>1]==0xB640u){src=(const int8_t*)(uintptr_t)hw_reg32[0xBC>>2];off=0;dma_on=1;}
  else if(hw_reg16[0xC6>>1]==0u){dma_on=0;}
  else hw_reg16[0xC6>>1]=before; }
static void waitvb_at(void){ settm3(); ps4_audio_prepare_frame(); uint64_t e=(t/SPF+1)*SPF; play_until(e); commit(); }
static void resamp(const int8_t* f,int8_t* o){unsigned si=0,acc=0;for(unsigned i=0;i<SPF;i++){o[i]=f[si];acc+=224;if(acc>=SPF){acc-=SPF;if(si<223)++si;}}}
static void judge(const char* tag, uint64_t t0, uint64_t t1){
  int8_t e[SPF]; long j=-1; unsigned ok=0,rep=0,bad=0,frames=0;
  for(uint64_t f=t0;f+SPF<=t1;f+=SPF){ frames++; long hit=-1;
    for(long k=(j<0?0:j); k<(long)rn && k<(j<0?(long)rn:j+3); k++){resamp(rlog[k],e); if(!memcmp(out+f,e,SPF)){hit=k;break;}}
    if(hit<0){bad++;continue;} if(j>=0&&hit==j)rep++; else ok++; j=hit; }
  printf("%-34s vblanks=%5u in-order=%5u repeats=%4u splice/garbage=%4u  garbage_bytes=%7ld  tempo=%.3f\n",
         tag,frames,ok,rep,bad,garbage,(double)rn/(double)((t1-t0)/SPF));
}
static double urand(void){ return (double)rand()/RAND_MAX; }
/* pattern: returns work (in frames) for iteration i */
static int scen;
static double work(unsigned i){
  switch(scen){
  case 0: return 0.55;                                   /* on time */
  case 1: return 1.15;                                   /* every frame a little late */
  case 2: return (i&1)?0.8:1.6;                          /* alternating */
  case 3: return i==300?30.0:0.55;                       /* one 30-frame load stall */
  case 4: return 0.7+1.5*urand();                        /* battle-like random 0.7..2.2 */
  default: return 0.55; }
}
int main(int argc,char** argv){
  unsigned music=argc>1?(unsigned)strtoul(argv[1],0,16):0x95u;
  scen=argc>2?atoi(argv[2]):0; unsigned iters=argc>3?(unsigned)atoi(argv[3]):1200;
  out=malloc(SPF*(MAXF+100)); srand(1234);
  ps4_audio_init(); ps4_audio_play((uint8_t)music);
  uint64_t t0=0;
  for(unsigned i=0;i<iters && t<(uint64_t)SPF*(MAXF-100);i++){
    uint64_t w=(uint64_t)(work(i)*SPF); play_until(t+w); waitvb_at(); if(i==2){t0=t; garbage=0;} }
  char tag[64]; snprintf(tag,sizeof tag,"music %02X scenario %d",music,scen); judge(tag,t0,t);
  if(argc>4){ /* lane profile */
    unsigned mf=0,mp=0,mn=0,md=0; double sf=0,sp=0,sd=0,sn=0; unsigned n=rn<MAXF?rn:MAXF;
    for(unsigned k=0;k<n;k++){ if(pf[k]>mf)mf=pf[k]; if(pp[k]>mp)mp=pp[k]; if(pn[k]>mn)mn=pn[k]; if(pd[k]>md)md=pd[k]; sf+=pf[k];sp+=pp[k];sd+=pd[k];sn+=pn[k]; }
    printf("  lanes/frame over %u frames: FM avg %.2f max %u | PSG avg %.2f max %u (noise avg %.2f) | DAC avg %.2f max %u\n",n,sf/n,mf,sp/n,mp,sn/n,sd/n,md);
  }
  return 0;
}
