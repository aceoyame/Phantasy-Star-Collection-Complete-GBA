#include <stdint.h>
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
#include "../gba/r28_readable_font.h"
#include "catalog.h"
static unsigned selected,first;
static void wait(void){while(R16(0x04000006)>=160){}while(R16(0x04000006)<160){}}
static void pixel(unsigned x,unsigned y,unsigned c){if(x>=240||y>=160)return;volatile uint16_t*p=(void*)(0x06000000+y*240+(x&~1u));unsigned q=*p;*p=(x&1)?(q&255)|(c<<8):(q&0xff00)|c;}
static void text(unsigned x,unsigned y,const char*s,unsigned c){while(*s){unsigned ch=*s++;unsigned i=ch>='a'&&ch<='z'?ch-'a'+1:ch>='A'&&ch<='Z'?ch-'A'+1:ch>='0'&&ch<='9'?ch-'0'+27:0;if(ch=='-'){for(unsigned n=0;n<4;n++)pixel(x+n,y+3,c);}else for(unsigned n=0;n<5;n++)for(unsigned b=0;b<7;b++)if(r28_font5x7[i][n]&(1<<b))pixel(x+n,y+b,c);x+=6;}}
static void draw(void){R32(0x040000d4)=(uint32_t)menu_background;R32(0x040000d8)=0x06000000;R32(0x040000dc)=0x84002580;
 for(unsigned row=0;row<7&&first+row<CATALOG_COUNT;row++){unsigned idx=first+row,y=45+row*12;if(idx==selected)text(8,y,"-",3);text(20,y,catalog[idx].title,idx==selected?3:2);}
 text(12,139,selected<2?"GAME GEAR":"MEGA DRIVE",3);text(12,151,"A LAUNCH    B COLLECTION",2);
 R16(0x04000000)=0x0404;}
static void reset(void){__asm__ volatile("mov r0,#0\n swi 0x10000\n swi 0":::"r0","memory");for(;;){}}
void menu_main(void){
 R16(0x04000084)=0;R16(0x04000020)=R16(0x04000026)=0x100;R16(0x04000022)=R16(0x04000024)=0;R32(0x04000028)=R32(0x0400002c)=0;
 volatile uint16_t*p=(void*)0x05000000;p[0]=0x0c41;p[1]=0x18a2;p[2]=0x7bde;p[3]=0x43df;p[4]=0x7eec;
 selected=first=0;draw();unsigned prev=1023;
 for(;;){wait();unsigned k=(~R16(0x04000130))&1023,edge=k&~prev;prev=k;if(edge&2)reset();
 if(edge&(64|128)){
  if(edge&64)selected=selected?selected-1:CATALOG_COUNT-1;
  else { selected++; if(selected>=CATALOG_COUNT)selected=0; }
  if(selected<first)first=selected;if(selected>=first+7)first=selected-6;draw();
 }
 /* A (or START) launches the selected entry */
 if((edge&(1|8))&&catalog[selected].table){volatile uint32_t*c=(void*)0x0203ff00;c[0]=catalog[selected].table;c[1]=catalog[selected].size;c[2]=catalog[selected].flags;c[3]=selected;c[4]=selected>=2?(uint32_t)catalog[selected].title:0;void(*entry)(void)=(void*)(selected<2?0x086ca500:0x086e2000);entry();}
 }
}
