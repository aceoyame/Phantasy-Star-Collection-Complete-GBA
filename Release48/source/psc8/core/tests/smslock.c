/* Lockstep: reference core vs z80f on a real cartridge with a Sega mapper; deterministic I/O stub. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/claude/work/z80ref/z80.h"
static uint8_t *cart; static unsigned cmask;
typedef struct { uint8_t ram[8192]; uint8_t banks[4]; unsigned ioc; } M;
static M m1, m2;
static uint8_t mrd(M*m,uint16_t a){ if(a>=0xc000) return m->ram[a&8191]; if(a<0x400) return cart[a];
  unsigned slot=a>>14; return cart[(((unsigned)m->banks[slot+1]<<14)&cmask)|(a&16383)]; }
static void mwr(M*m,uint16_t a,uint8_t v){ if(a>=0xc000){m->ram[a&8191]=v; if(a>=0xfffc) m->banks[a&3]=v;} }
static uint8_t io(M*m,uint8_t p){ m->ioc++; if((p&0xc1)==0x81) return (m->ioc&7)==0?0x80:0x00; if((p&0xc0)==0x40) return (m->ioc*7)&255; return 0xff; }
static int done;
#define RD8(a) mrd(&m2,(a))
#define WR8(a,v) mwr(&m2,(a),(v))
#define PORT_IN(p) io(&m2,(p))
#define PORT_OUT(p,v) (m2.ioc++)
#define Z80F_STATIC static
#include "../z80f.h"
static uint8_t rb(void*u,uint16_t a){return mrd(&m1,a);}
static void wb(void*u,uint16_t a,uint8_t v){mwr(&m1,a,v);}
static uint8_t in(z80*z,uint8_t p){return io(&m1,p);}
static void out(z80*z,uint8_t p,uint8_t v){m1.ioc++;}
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"rb"); fseek(f,0,2); long n=ftell(f); rewind(f); cart=malloc(n); fread(cart,1,n,f); fclose(f); cmask=n-1;
  m1.banks[1]=0;m1.banks[2]=1;m1.banks[3]=2; m2=m1;
  z80 a; z80_init(&a); a.read_byte=rb;a.write_byte=wb;a.port_in=in;a.port_out=out;
  Z80F b; z80f_reset(&b); b.f=0xff;
  for(long i=0;i<atol(argv[2]);i++){
    uint16_t pc=a.pc; unsigned long c0=a.cyc; uint32_t c1=b.cyc;
    int irq=(i%50000==49999);
    if(irq) z80_gen_int(&a,0xff);
    z80_step(&a); z80f_run(&b,b.cyc+1);
    if(irq){ if(b.iff1){ b.int_pending=1; b.int_data=0xff; z80f_run(&b,b.cyc+1);} a.int_pending=0; b.int_pending=0; }
    uint8_t fa=(a.sf<<7)|(a.zf<<6)|(a.yf<<5)|(a.hf<<4)|(a.xf<<3)|(a.pf<<2)|(a.nf<<1)|a.cf;
    if(a.pc!=((b.pc+b.halted)&0xffff)||a.a!=b.a||(fa&0xd7)!=(b.f&0xd7)||a.b!=b.b||a.c!=b.c||a.d!=b.d||a.e!=b.e||a.h!=b.h||a.l!=b.l||a.sp!=b.sp||a.ix!=b.ix||a.iy!=b.iy||(uint32_t)(a.cyc-c0)!=b.cyc-c1){
      printf("diverge at %ld pc=%04x op=%02x %02x %02x: ref pc=%04x a=%02x f=%02x bc=%02x%02x hl=%02x%02x sp=%04x cyc=%lu | mine pc=%04x a=%02x f=%02x bc=%02x%02x hl=%02x%02x sp=%04x cyc=%u\n",
        i,pc,mrd(&m1,pc),mrd(&m1,pc+1),mrd(&m1,pc+2),a.pc,a.a,fa,a.b,a.c,a.h,a.l,a.sp,a.cyc-c0,b.pc,b.a,b.f,b.b,b.c,b.h,b.l,b.sp,b.cyc-c1);
      return 1; }
    if(a.int_pending==0) b.int_pending=0;
  }
  printf("no divergence\n"); return 0;
}
