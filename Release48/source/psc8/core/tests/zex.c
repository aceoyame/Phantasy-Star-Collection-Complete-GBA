#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char mem[65536]; static int done;
#define RD8(a) mem[(a)]
#define WR8(a,v) (mem[(a)]=(unsigned char)(v))
static unsigned zin(unsigned c,unsigned d,unsigned e);
#define PORT_IN(p) zin(z->c,z->d,z->e)
#define PORT_OUT(p,v) (done=1)
#define Z80F_STATIC static
#include "../z80f.h"
static unsigned zin(unsigned c,unsigned d,unsigned e){
  if(c==2) putchar(e);
  else if(c==9){unsigned a=(d<<8)|e; while(mem[a]!='$') putchar(mem[a++]);}
  fflush(stdout); return 0xff;
}
int main(int argc,char**argv){
  FILE*f=fopen(argv[1],"rb"); size_t n=fread(mem+0x100,1,65536-0x100,f); fclose(f); (void)n;
  mem[0]=0xD3; mem[1]=0x00; mem[5]=0xDB; mem[6]=0x00; mem[7]=0xC9;
  Z80F z; z80f_reset(&z); z.pc=0x100;
  unsigned long long total=0;
  while(!done){ z80f_run(&z, z.cyc+997); }
  total=z.cyc;
  printf("\n[done ~%llu cycles]\n", total); return 0;
}
