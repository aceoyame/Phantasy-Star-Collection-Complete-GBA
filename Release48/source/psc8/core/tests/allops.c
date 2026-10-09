#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/claude/work/z80ref/z80.h"
static uint8_t m1[65536], m2[65536];
#define RD8(a) m2[(a)]
#define WR8(a,v) (m2[(a)]=(uint8_t)(v))
#define PORT_IN(p) 0x5a
#define PORT_OUT(p,v) ((void)0)
#define Z80F_STATIC static
#include "../z80f.h"
static uint8_t rb(void*u,uint16_t a){return m1[a];}
static void wb(void*u,uint16_t a,uint8_t v){m1[a]=v;}
static uint8_t in(z80*z,uint8_t p){return 0x5a;}
static void out(z80*z,uint8_t p,uint8_t v){}
static int test(uint8_t *code,int n,unsigned seed){
  srand(seed); for(int i=0;i<65536;i++) m1[i]=rand();
  memcpy(m1+0x8000,code,n); memcpy(m2,m1,65536);
  z80 a; z80_init(&a); a.read_byte=rb;a.write_byte=wb;a.port_in=in;a.port_out=out;
  a.pc=0x8000; a.sp=0x4000+rand()%256; a.a=rand();a.b=rand();a.c=rand();a.d=rand();a.e=rand();a.h=rand();a.l=rand();a.ix=rand();a.iy=rand();
  uint8_t f=rand(); a.sf=f>>7&1;a.zf=f>>6&1;a.yf=f>>5&1;a.hf=f>>4&1;a.xf=f>>3&1;a.pf=f>>2&1;a.nf=f>>1&1;a.cf=f&1;
  Z80F b; z80f_reset(&b); b.pc=a.pc;b.sp=a.sp;b.a=a.a;b.b=a.b;b.c=a.c;b.d=a.d;b.e=a.e;b.h=a.h;b.l=a.l;b.ix=a.ix;b.iy=a.iy;b.f=f;
  z80_step(&a); z80f_run(&b,1);
  uint8_t fa=(a.sf<<7)|(a.zf<<6)|(a.yf<<5)|(a.hf<<4)|(a.xf<<3)|(a.pf<<2)|(a.nf<<1)|a.cf;
  int bad = a.pc!=((b.pc+b.halted)&0xffff)||a.a!=b.a||(fa&0xd7)!=(b.f&0xd7)||a.b!=b.b||a.c!=b.c||a.d!=b.d||a.e!=b.e||a.h!=b.h||a.l!=b.l||a.sp!=b.sp||a.ix!=b.ix||a.iy!=b.iy||a.cyc!=b.cyc||memcmp(m1,m2,65536);
  if(bad) printf("%02x %02x %02x %02x: ref pc=%04x cyc=%lu f=%02x | mine pc=%04x cyc=%u f=%02x mem=%d\n",code[0],code[1],code[2],code[3],a.pc,a.cyc,fa,b.pc,b.cyc,b.f,memcmp(m1,m2,65536)!=0);
  return bad;
}
int main(){
  int bad=0; uint8_t c[8];
  for(int p=0;p<6;p++) for(int o=0;o<256;o++){
    int pre[6][2]={{-1,-1},{0xCB,-1},{0xED,-1},{0xDD,-1},{0xFD,-1},{0xDD,0xCB}};
    int n=0; if(pre[p][0]>=0)c[n++]=pre[p][0]; if(pre[p][1]>=0){c[n++]=pre[p][1];c[n++]=0x05;} c[n++]=o; c[n++]=0x12; c[n++]=0x34; c[n++]=0x56;
    if(p<5 && (o==0xCB||o==0xED||o==0xDD||o==0xFD)&&p!=1&&p!=2) continue;
    for(unsigned s=1;s<=20;s++) if(test(c,n,s*7919+o)){bad++;break;}
  }
  printf("bad=%d\n",bad); return 0;
}
