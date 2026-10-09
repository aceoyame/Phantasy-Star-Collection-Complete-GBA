#include <stdint.h>
#include <stddef.h>
#include "fast_opll.h"
void *memset(void *p,int c,size_t n){uint8_t *d=p;while(n--)*d++=c;return p;}
void *memcpy(void *p,const void *s,size_t n){uint8_t *d=p;const uint8_t *q=s;while(n--)*d++=*q++;return p;}
int memcmp(const void*a,const void*b,size_t n){const uint8_t*x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;x++;y++;}return 0;}
void __aeabi_memcpy(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memcpy4(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memclr(void *p,size_t n){memset(p,0,n);}
void __aeabi_memclr4(void *p,size_t n){memset(p,0,n);}
static unsigned rs=1; static unsigned rnd(void){rs=rs*1103515245u+12345u;return rs>>16;}
#define RES ((volatile uint32_t*)0x0203E000)
void fmt_main(void){
  unsigned long bad=0,tot=0;
  for(int trial=0;trial<40;trial++){
    static FastOPLL a,b; fastopll_reset(&a); fastopll_reset(&b);
    for(int blk=0;blk<100;blk++){
      int nw=rnd()%4;
      for(int w=0;w<nw;w++){ unsigned reg=rnd()%64, v=rnd()&255;
        if(rnd()%3==0) reg=0x20+rnd()%9; if(rnd()%4==0) reg=0x30+rnd()%9; if(rnd()%5==0) reg=0x0e;
        if(reg==0x0e && trial%3) v&=0x1f;
        fastopll_addr(&a,reg); fastopll_data(&a,v); fastopll_addr(&b,reg); fastopll_data(&b,v); }
      unsigned n=1+rnd()%300; static int16_t acc[400];
      fastopll_render(&b,acc,n);
      for(unsigned i=0;i<n;i++){ int y=acc[i]>>3; y=y>127?127:y<-128?-128:y; int x=fastopll_calc(&a); tot++; if(x!=y) bad++; }
      if(memcmp(&a,&b,FASTOPLL_STATE_BYTES)) bad+=1000000;
    }
    RES[1]=tot; RES[2]=bad;
  }
  RES[0]=0x464D5431; for(;;){}
}
