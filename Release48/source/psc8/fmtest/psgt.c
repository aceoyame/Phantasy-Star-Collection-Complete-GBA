#include <stdint.h>
#include <stddef.h>
void *memset(void *p,int c,size_t n){uint8_t *d=p;while(n--)*d++=c;return p;}
void *memcpy(void *p,const void *s,size_t n){uint8_t *d=p;const uint8_t *q=s;while(n--)*d++=*q++;return p;}
int memcmp(const void*a,const void*b,size_t n){const uint8_t*x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;x++;y++;}return 0;}
void __aeabi_memcpy(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memcpy4(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memclr(void *p,size_t n){memset(p,0,n);}
void __aeabi_memclr4(void *p,size_t n){memset(p,0,n);}
#define HOT
#define PSG_TONE_ASM
#include "psg.h"
static unsigned rs=7; static unsigned rnd(void){rs=rs*1103515245u+12345u;return rs>>16;}
#define RES ((volatile uint32_t*)0x0203E000)
static int16_t L1[400],R1[400],L2[400],R2[400];
void fmt_main(void){
  unsigned long pbad=0,ptot=0;
  for(int trial=0;trial<60;trial++){
    noise_lfsr=0x8000; memset(tones,0,sizeof tones); memset(volumes,15,sizeof volumes); memset(phases,0,sizeof phases); noise=0; psg_latch=0; psg_clock_fraction=0;
    for(int blk=0;blk<60;blk++){
      int nw=rnd()%4; for(int w=0;w<nw;w++) psg_write(rnd()&255);
      unsigned n=1+rnd()%300, mask=rnd()&255, en=rnd()%4!=0;
      uint16_t t0[3]; uint8_t v0[4]; uint32_t ph0[4]; uint16_t lf0=noise_lfsr; uint8_t nz0=noise, fr0=psg_clock_fraction;
      memcpy(t0,tones,sizeof t0); memcpy(v0,volumes,4); memcpy(ph0,phases,sizeof ph0);
      for(unsigned i=0;i<n;i++){ int l=0,r=0; uint32_t step=psg_step(); for(unsigned c=0;c<4;c++){int s=c<3?psg_tone(c,step):psg_noise(step); if(en){if(mask&(16u<<c))l+=s; if(mask&(1u<<c))r+=s;}} L1[i]=l; R1[i]=r; }
      uint32_t ph1[4]; memcpy(ph1,phases,sizeof ph1); uint16_t lf1=noise_lfsr; uint8_t fr1=psg_clock_fraction;
      memcpy(tones,t0,sizeof t0); memcpy(volumes,v0,4); memcpy(phases,ph0,sizeof ph0); noise_lfsr=lf0; noise=nz0; psg_clock_fraction=fr0;
      memset(L2,0,sizeof L2); memset(R2,0,sizeof R2); psg_render(L2,R2,n,mask,en);
      for(unsigned i=0;i<n;i++){ ptot++; if(L1[i]!=L2[i]||R1[i]!=R2[i]) pbad++; }
      if(memcmp(ph1,phases,sizeof ph1)||lf1!=noise_lfsr||fr1!=psg_clock_fraction) pbad+=1000000;
    }
    RES[1]=ptot; RES[2]=pbad;
  }
  RES[0]=0x50534731; for(;;){}
}
