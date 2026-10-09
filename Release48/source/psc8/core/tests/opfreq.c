#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *cart; static unsigned cmask;
typedef struct { uint8_t ram[8192]; uint8_t banks[4]; unsigned ioc; } M;
static M m2; static unsigned long freq[4][256];
static uint8_t mrd(M*m,uint16_t a){ if(a>=0xc000) return m->ram[a&8191]; if(a<0x400) return cart[a];
  unsigned slot=a>>14; return cart[(((unsigned)m->banks[slot+1]<<14)&cmask)|(a&16383)]; }
static void mwr(M*m,uint16_t a,uint8_t v){ if(a>=0xc000){m->ram[a&8191]=v; if(a>=0xfffc) m->banks[a&3]=v;} }
static uint8_t io(M*m,uint8_t p){ m->ioc++; if((p&0xc1)==0x81) return (m->ioc&7)==0?0x80:0x00; if((p&0xc0)==0x40) return (m->ioc*7)&255; return 0xff; }
#define RD8(a) mrd(&m2,(a))
#define WR8(a,v) mwr(&m2,(a),(v))
#define PORT_IN(p) io(&m2,(p))
#define PORT_OUT(p,v) (m2.ioc++)
#define Z80F_STATIC static
#include "../z80f.h"
int main(int argc,char**argv){
  for(int k=1;k<argc;k++){
  FILE*f=fopen(argv[k],"rb"); fseek(f,0,2); long n=ftell(f); rewind(f); cart=malloc(n); fread(cart,1,n,f); fclose(f); cmask=n-1;
  memset(&m2,0,sizeof m2); m2.banks[1]=0;m2.banks[2]=1;m2.banks[3]=2;
  Z80F b; z80f_reset(&b);
  for(long i=0;i<20000000;i++){
    uint8_t o=mrd(&m2,b.pc), o2=mrd(&m2,(b.pc+1)&0xffff);
    if(o==0xCB) freq[1][o2]++; else if(o==0xED) freq[2][o2]++; else if(o==0xDD||o==0xFD) freq[3][o2]++; else freq[0][o]++;
    if(i%30000==29999){ b.int_pending=1; b.int_data=0xff; }
    z80f_run(&b,b.cyc+1);
  }}
  const char*nm[4]={"main","cb","ed","xy"};
  for(int t=0;t<4;t++){ unsigned long tot=0; for(int i=0;i<256;i++) tot+=freq[t][i]; printf("%s %lu\n",nm[t],tot);}
  FILE*o=fopen("opfreq.txt","w"); for(int t=0;t<4;t++) for(int i=0;i<256;i++) fprintf(o,"%s %02x %lu\n",nm[t],i,freq[t][i]); fclose(o);
  return 0;
}
