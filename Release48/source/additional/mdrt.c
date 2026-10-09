/* Freestanding runtime helpers for the Genesis engine: memory ops and integer
 * division.  Word-at-a-time where alignment allows; lives in IWRAM. */
#include <stdint.h>
#include <stddef.h>

void *memset(void*p,int c,size_t n){
  uint8_t*d=p; uint32_t v=(uint8_t)c;
  while(n&&((uintptr_t)d&3u)){ *d++=(uint8_t)v; n--; }
  v|=v<<8; v|=v<<16;
  uint32_t*w=(uint32_t*)d; while(n>=16){ w[0]=v; w[1]=v; w[2]=v; w[3]=v; w+=4; n-=16; }
  while(n>=4){ *w++=v; n-=4; }
  d=(uint8_t*)w; while(n--) *d++=(uint8_t)v;
  return p;
}
void *memcpy(void*p,const void*s,size_t n){
  uint8_t*d=p; const uint8_t*q=s;
  if((((uintptr_t)d|(uintptr_t)q)&3u)==0u){
    uint32_t*dw=(uint32_t*)d; const uint32_t*sw=(const uint32_t*)q;
    while(n>=16){ uint32_t a=sw[0],b=sw[1],c=sw[2],e=sw[3]; dw[0]=a; dw[1]=b; dw[2]=c; dw[3]=e; dw+=4; sw+=4; n-=16; }
    while(n>=4){ *dw++=*sw++; n-=4; }
    d=(uint8_t*)dw; q=(const uint8_t*)sw;
  }
  while(n--) *d++=*q++;
  return p;
}
void *memmove(void*p,const void*s,size_t n){
  uint8_t*d=p; const uint8_t*q=s;
  if(d<=q||d>=q+n){ return memcpy(p,s,n); }
  while(n--) d[n]=q[n];
  return p;
}
int memcmp(const void*a,const void*b,size_t n){
  const uint8_t*x=a,*y=b; while(n--){ if(*x!=*y) return (int)*x-(int)*y; x++; y++; } return 0;
}
void __aeabi_memcpy(void*p,const void*s,size_t n){ memcpy(p,s,n); }
void __aeabi_memcpy4(void*p,const void*s,size_t n){ memcpy(p,s,n); }
void __aeabi_memcpy8(void*p,const void*s,size_t n){ memcpy(p,s,n); }
void __aeabi_memmove(void*p,const void*s,size_t n){ memmove(p,s,n); }
void __aeabi_memmove4(void*p,const void*s,size_t n){ memmove(p,s,n); }
void __aeabi_memset(void*p,size_t n,int c){ memset(p,c,n); }
void __aeabi_memset4(void*p,size_t n,int c){ memset(p,c,n); }
void __aeabi_memset8(void*p,size_t n,int c){ memset(p,c,n); }
void __aeabi_memclr(void*p,size_t n){ memset(p,0,n); }
void __aeabi_memclr4(void*p,size_t n){ memset(p,0,n); }
void __aeabi_memclr8(void*p,size_t n){ memset(p,0,n); }

/* Shift-subtract division; used by the reference core's DIVU/DIVS and the idle-spin skip. */
unsigned __aeabi_uidiv(unsigned n,unsigned d){
  unsigned q=0,b=1; if(!d) return 0;
  while(d<=n&&!(d&0x80000000u)){ d<<=1; b<<=1; }
  while(b){ if(n>=d){ n-=d; q|=b; } d>>=1; b>>=1; }
  return q;
}
int __aeabi_idiv(int n,int d){
  unsigned un=n<0?0u-(unsigned)n:(unsigned)n, ud=d<0?0u-(unsigned)d:(unsigned)d;
  unsigned q=__aeabi_uidiv(un,ud); return ((n<0)^(d<0))?(int)(0u-q):(int)q;
}
