/* host: fastopll_render(n) must equal n x fastopll_calc(); psg_render must equal the per-sample PSG path */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HOT
#include "../fast_opll.h"
#include "../psg.h"
static unsigned rs=1; static unsigned rnd(void){rs=rs*1103515245u+12345u;return rs>>16;}
int main(void){
  long bad=0, tot=0;
  for(int trial=0;trial<300;trial++){
    FastOPLL a,b; fastopll_reset(&a); fastopll_reset(&b);
    for(int blk=0;blk<200;blk++){
      int nw=rnd()%4;
      for(int w=0;w<nw;w++){ unsigned reg=rnd()%64, v=rnd()&255;
        if(rnd()%3==0) reg=0x20+rnd()%9; if(rnd()%4==0) reg=0x30+rnd()%9; if(rnd()%5==0) reg=0x0e;
        if(reg==0x0e && trial%3) v&=0x1f;
        fastopll_addr(&a,reg); fastopll_data(&a,v); fastopll_addr(&b,reg); fastopll_data(&b,v); }
      unsigned n=1+rnd()%300; int16_t acc[400];
      fastopll_render(&b,acc,n);
      for(unsigned i=0;i<n;i++){ int y=acc[i]>>3; y=y>127?127:y<-128?-128:y; acc[i]=(int16_t)y; }
      for(unsigned i=0;i<n;i++){ int x=fastopll_calc(&a); tot++; if(x!=acc[i]){ if(bad<5) printf("FM mismatch trial %d blk %d i %u: %d vs %d\n",trial,blk,i,x,acc[i]); bad++; } }
      if(memcmp(&a,&b,FASTOPLL_STATE_BYTES)){ if(bad<5) printf("FM state differs trial %d blk %d\n",trial,blk); bad++; }
    }
  }
  printf("FM samples %ld mismatches %ld\n",tot,bad);
  /* PSG */
  long pbad=0,ptot=0;
  for(int trial=0;trial<200;trial++){
    noise_lfsr=0x8000; memset(tones,0,sizeof tones); memset(volumes,15,sizeof volumes); memset(phases,0,sizeof phases); noise=0; psg_latch=0; psg_clock_fraction=0;
    for(int blk=0;blk<100;blk++){
      int nw=rnd()%4; for(int w=0;w<nw;w++) psg_write(rnd()&255);
      unsigned n=1+rnd()%300, mask=rnd()&255, en=rnd()%4!=0;
      /* save state, run per-sample path */
      uint16_t t0[3]; uint8_t v0[4]; uint32_t ph0[4]; uint16_t lf0=noise_lfsr; uint8_t nz0=noise, fr0=psg_clock_fraction;
      memcpy(t0,tones,sizeof t0); memcpy(v0,volumes,4); memcpy(ph0,phases,sizeof ph0);
      int16_t L1[400],R1[400],L2[400],R2[400];
      for(unsigned i=0;i<n;i++){ int l=0,r=0; uint32_t step=psg_step(); for(unsigned c=0;c<4;c++){int s=c<3?psg_tone(c,step):psg_noise(step); if(en){if(mask&(16u<<c))l+=s; if(mask&(1u<<c))r+=s;}} L1[i]=l; R1[i]=r; }
      uint32_t ph1[4]; memcpy(ph1,phases,sizeof ph1); uint16_t lf1=noise_lfsr; uint8_t fr1=psg_clock_fraction;
      /* restore and run block path */
      memcpy(tones,t0,sizeof t0); memcpy(volumes,v0,4); memcpy(phases,ph0,sizeof ph0); noise_lfsr=lf0; noise=nz0; psg_clock_fraction=fr0;
      memset(L2,0,sizeof L2); memset(R2,0,sizeof R2); psg_render(L2,R2,n,mask,en);
      for(unsigned i=0;i<n;i++){ ptot++; if(L1[i]!=L2[i]||R1[i]!=R2[i]){ if(pbad<5) printf("PSG mismatch %d %d %u: %d/%d vs %d/%d\n",trial,blk,i,L1[i],R1[i],L2[i],R2[i]); pbad++; } }
      if(memcmp(ph1,phases,sizeof ph1)||lf1!=noise_lfsr||fr1!=psg_clock_fraction){ if(pbad<5) printf("PSG state differs %d %d\n",trial,blk); pbad++; }
    }
  }
  printf("PSG samples %ld mismatches %ld\n",ptot,pbad);
  return bad||pbad;
}
