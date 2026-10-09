/* CP11: listening panel over PSC's original PSIII sound engine.
 * Enter with L+R at launch. Only the panel mutes the six already-rendered
 * log-sample buffers; phase, envelopes and scripts continue normally. */
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
#define R16(a) (*(volatile u16*)(a))
#define R32(a) (*(volatile u32*)(a))
#include "../ps2dbg/font5x7.h"
struct State { u32 magic, song, playing, voice, mute; };
#define D ((volatile struct State*)0x0203C000)
#define MAGIC 0x44335350u
#define MUSIC (*(volatile int32_t*)0x0201F3C4)
static u32 held(void){ return (~R16(0x04000130))&1023u; }
static void wait(void){ while(R16(0x04000006)>=160){} while(R16(0x04000006)<160){} }
static void px(u32 x,u32 y,u32 c){
 volatile u16*p=(volatile u16*)(0x06000000+y*240+(x&~1u)); u32 v=*p;
 *p=(u16)((x&1u)?((v&255u)|(c<<8)):((v&0xff00u)|c));
}
static void text(u32 x,u32 y,const char*s,u32 c){
 while(*s&&x<234u){ u32 ch=(u8)*s++; if(ch<32||ch>126)ch='?';
  for(u32 b=0;b<7;b++)for(u32 n=0;n<5;n++)if(font5x7[ch-32][n]&(1u<<b))px(x+n,y+b,c);
  x+=6;
 }
}
static void number(u32 x,u32 y,u32 n){
 volatile u32 rest=n;char s[4]={'0','0','0',0};
 while(rest>=10){s[1]++;rest-=10;}s[2]+=rest;text(x,y,s,2);
}
static void draw(void){
 for(u32 i=0;i<9600;i++)R32(0x06000000+i*4)=0;
 text(12,12,"PHANTASY STAR III",3); text(12,24,"SOUND TEST",2);
 text(12,45,"PSC SONG",1); number(78,45,D->song);
 text(12,57,"GENESIS SOURCE",1); number(108,57,D->song-1);
 text(12,73,D->playing?"PLAYING":"STOPPED",2);
 text(12,89,"VOICES",1);
 for(u32 v=0;v<6;v++){
  u32 x=60+v*28;char s[2]={'1'+v,0};text(x,89,s,D->voice==v?3:1);
  text(x,101,(D->mute&(1u<<v))?"OFF":"ON",(D->mute&(1u<<v))?4:2);
 }
 text(12,118,"UP/DOWN SONG   A PLAY   START STOP",1);
 text(12,130,"LEFT/RIGHT VOICE   SELECT MUTE",1);
 text(12,146,"B START GAME",2);
}
static void restart(void){
 D->magic=0; D->mute=0; R16(0x04000208)=0;R16(0x04000200)=0;
 R16(0x04000102)=0;R16(0x040000c6)=0;R16(0x04000084)=0;
 __asm__ volatile("movs r0,#0xFF\n swi 1":::"r0","memory");
 ((void(*)(void))0x08080000)();for(;;){}
}
void ps3dbg_hook(void){
 D->magic=0;D->mute=0;
 if((held()&768u)!=768u)return;
 D->magic=MAGIC;D->song=1;D->playing=0;D->voice=0;
 R32(0x0201858C)=1; /* VBlank video lock: audio keeps running. */
 R16(0x04000000)=0x80;R16(0x0400000c)=0;
 R16(0x04000020)=R16(0x04000026)=256;R16(0x04000022)=R16(0x04000024)=0;
 R32(0x04000028)=R32(0x0400002c)=0;
 R16(0x04000048)=R16(0x0400004a)=R16(0x04000050)=R16(0x0400004c)=0;
 const u16 pal[5]={0x2000,0x7fff,0x2fe5,0x03ff,0x001f};
 for(u32 i=0;i<5;i++)R16(0x05000000+2*i)=pal[i];
 R16(0x04000000)=0x0404;draw();u32 prev=held();
 for(;;){
  wait();u32 k=held(),edge=k&~prev;prev=k;u32 dirty=0;
  if(edge&2)restart();
  if(edge&64){D->song=D->song>1?D->song-1:42;dirty=1;}
  if(edge&128){D->song=D->song<42?D->song+1:1;dirty=1;}
  if(edge&32){D->voice=D->voice?D->voice-1:5;dirty=1;}
  if(edge&16){D->voice=D->voice<5?D->voice+1:0;dirty=1;}
  if(edge&4){D->mute^=1u<<D->voice;dirty=1;}
  if(edge&1){MUSIC=(int32_t)(0x80u+D->song);D->playing=D->song;dirty=1;}
  if(edge&8){MUSIC=0x80;D->playing=0;dirty=1;}
  if(dirty)draw();
 }
}
/* Called immediately before the original mixer at IWRAM 0x03001000.
 * Six 352-byte log-sample buffers start at 0x03000000; log code 0 is silence.
 * It affects audition output only. Sequencers and SFX state are untouched. */
void ps3dbg_mute(void){
 if(D->magic!=MAGIC||!D->mute)return;
 for(u32 v=0;v<6;v++)if(D->mute&(1u<<v)){
  volatile u32*p=(volatile u32*)(0x03000000+352*v);
  for(u32 i=0;i<88;i++)p[i]=0;
 }
}
