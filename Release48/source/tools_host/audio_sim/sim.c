#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "ps4_audio_runtime.h"
volatile uint16_t hw_reg16[0x200]; volatile uint32_t hw_reg32[0x100];
#define SPF 528u
static uint32_t tick; static const int8_t* dma_src; static unsigned dma_off, commits, overruns;
static int8_t outbuf[SPF*4000]; static unsigned outn;
static void settick(void){ hw_reg16[0x10C>>1]=(uint16_t)tick; }
static void consume_frame(void){ /* FIFO plays one display frame */
  tick+=4389u; settick();
  for(unsigned i=0;i<SPF;i++){ int8_t s=0; if(dma_src){ if(dma_off>=SPF*3u){overruns++;} else s=dma_src[dma_off]; dma_off++; } outbuf[outn++]=s; }
}
static void commit(void){ hw_reg32[0xBC>>2]=0; hw_reg16[0xC6>>1]=0; ps4_audio_vblank_commit();
  if(hw_reg16[0xC6>>1]==0xB640u){ dma_src=(const int8_t*)(uintptr_t)hw_reg32[0xBC>>2]; dma_off=0; commits++; } }
static void waitvb_frame(void){ settick(); ps4_audio_prepare_frame(); consume_frame(); commit(); }
/* reference decode of a packed track */
extern const uint8_t ps4_pcm_track_87_start[], ps4_pcm_track_84_start[];
static int8_t* ref_decode(const uint8_t* d,unsigned w,uint8_t cur,unsigned n){int8_t*o=malloc(n);uint32_t bp=0;
  for(unsigned i=0;i<n;i++){o[i]=(int8_t)cur; const uint8_t*p=d+(bp>>3);uint32_t v=p[0]|(p[1]<<8)|(p[2]<<16);unsigned c=(v>>(bp&7))&((1u<<w)-1);bp+=w;
    int dl; if(c==(1u<<w)-1){p=d+(bp>>3);v=p[0]|(p[1]<<8)|(p[2]<<16);dl=(int8_t)((v>>(bp&7))&0xFF);bp+=8;} else dl=(int)c-(int)((1u<<(w-1))-1); cur=(uint8_t)(cur+dl);} return o;}
static void report(const char* tag,unsigned from,unsigned frames,const int8_t* ref){
  unsigned nz=0,match_at=~0u; for(unsigned i=from;i<from+frames*SPF;i++) if(outbuf[i]) nz++;
  /* find first frame from which output equals ref[0..] contiguously for 20 frames */
  for(unsigned f=0; f<frames && match_at==~0u; f++){ unsigned s=from+f*SPF; if(!memcmp(outbuf+s,ref,20*SPF)) match_at=f; }
  printf("%-28s frames=%u nonzero=%u/%u  matches_track_from_frame=%d\n",tag,frames,nz,frames*SPF,(int)match_at);
}
int main(int argc,char**argv){
  unsigned stall=argc>1?atoi(argv[1]):6, play_at=argc>2?atoi(argv[2]):3;
  int8_t* r87=ref_decode(ps4_pcm_track_87_start,6,0,SPF*200);
  int8_t* r84=ref_decode(ps4_pcm_track_84_start,5,19,SPF*200);
  settick(); ps4_audio_init(); ps4_audio_play(0x84);
  unsigned s0=outn; for(int f=0;f<100;f++) waitvb_frame(); report("town $84 steady",s0,60,r84);
  /* door transition: FORCE_BLANK map load stalls the main loop for `stall` frames;
     the $87 request happens `play_at` frames into the stall (after ps4_load_field_map) */
  unsigned s1=outn; for(unsigned f=0;f<stall;f++){ if(f==play_at){settick(); ps4_audio_play(0x87);} consume_frame(); if(f==play_at) {} }
  for(int f=0;f<150;f++) waitvb_frame();
  report("after shop $87 request",s1,120,r87);
  printf("commits=%u overruns(samples past guard)=%u current_music=%02X\n",commits,overruns,ps4_audio_current_music());
  return 0;}
