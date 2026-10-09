#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "m68k.h"
static uint8_t mem[65536];
u8 m68k_read8(u32 a){return mem[a&65535];}u16 m68k_read16(u32 a){return ((u16)mem[a&65535]<<8)|mem[(a+1)&65535];}u32 m68k_read32(u32 a){return ((u32)m68k_read16(a)<<16)|m68k_read16(a+2);}
void m68k_write8(u32 a,u8 v){mem[a&65535]=v;}void m68k_write16(u32 a,u16 v){m68k_write8(a,v>>8);m68k_write8(a+1,v);}void m68k_write32(u32 a,u32 v){m68k_write16(a,v>>16);m68k_write16(a+2,v);}
#include "idle.h"
int main(void){unsigned n=0;for(unsigned start=0;start<6;start+=2)for(unsigned timer=0;timer<3;timer++)for(unsigned end=1;end<2048;end++){
 memset(mem,0,sizeof mem);m68k_write16(0x3446,0x4a78);m68k_write16(0x3448,0xff96);m68k_write16(0x344a,0x66fa);m68k_write16(0x344c,0x4e71);m68k_write16(0xffff96,timer);memset(&C,0,sizeof C);C.pc=0x3446+start;C.sr=0x2700;
 M68K initial=C;while(C.cycles<end)m68k_step();M68K scalar=C;C=initial;while(C.cycles<end){md_fold_ram_wait(end);if(C.cycles<end)m68k_step();}assert(!memcmp(&C,&scalar,sizeof C));n++;
 }printf("PASS %u full CPU-state and exact-cycle wait-loop comparisons\n",n);}
