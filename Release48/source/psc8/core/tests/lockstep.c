#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "/home/claude/work/z80ref/z80.h"
static unsigned char m1[65536], m2[65536]; static int done;
#define RD8(a) m2[(a)]
#define WR8(a,v) (m2[(a)]=(unsigned char)(v))
#define PORT_IN(p) 0xff
#define PORT_OUT(p,v) (done=1)
#define Z80F_STATIC static
#include "../z80f.h"
static uint8_t rb(void*u,uint16_t a){return m1[a];}
static void wb(void*u,uint16_t a,uint8_t v){m1[a]=v;}
static uint8_t in(z80*z,uint8_t p){return 0xff;}
static void out(z80*z,uint8_t p,uint8_t v){}
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"rb"); size_t n=fread(m1+0x100,1,65536-0x100,f); fclose(f);
  m1[0]=0xD3; m1[5]=0xDB; m1[7]=0xC9; memcpy(m2,m1,65536);
  z80 a; z80_init(&a); a.read_byte=rb;a.write_byte=wb;a.port_in=in;a.port_out=out; a.pc=0x100;
  Z80F b; z80f_reset(&b); b.pc=0x100; b.cyc=0;
  int seen[65536]={0};
  for(long i=0;i<2000000000L;i++){
    uint16_t pc=a.pc; uint8_t o0=m1[pc],o1=m1[(pc+1)&0xffff],o2=m1[(pc+2)&0xffff],o3=m1[(pc+3)&0xffff];
    unsigned long c0=a.cyc; uint32_t c1=b.cyc;
    z80_step(&a); z80f_run(&b,b.cyc+1);
    if((uint32_t)(a.cyc-c0)!=b.cyc-c1 || a.pc!=b.pc){
      unsigned key=(o0<<8)|o1; if(1){printf("op %02x %02x %02x %02x ref=%lu mine=%u pc %04x/%04x\n",o0,o1,o2,o3,a.cyc-c0,b.cyc-c1,a.pc,b.pc);}
      b.cyc=c1+(uint32_t)(a.cyc-c0); if(a.pc!=b.pc) return 1;
    }
    if(a.pc==0) break;
  }
  return 0;
}
