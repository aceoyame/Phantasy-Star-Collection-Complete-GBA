/* Minimal headless GBA emulator for PSIV port testing.
 * ARM7TDMI interpreter + scanline PPU + DMA/timers/IRQ + HLE BIOS SWIs.
 * Not cycle accurate. Built as a shared library driven from Python (ctypes). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bios_bin.h"

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32; typedef int16_t s16; typedef uint64_t u64; typedef int64_t s64;

enum { M_USR=0x10, M_FIQ=0x11, M_IRQ=0x12, M_SVC=0x13, M_ABT=0x17, M_UND=0x1B, M_SYS=0x1F };
#define FN (1u<<31)
#define FZ (1u<<30)
#define FC (1u<<29)
#define FV (1u<<28)
#define FI (1u<<7)
#define FT (1u<<5)

typedef struct {
  u32 r[16]; u32 cpsr, spsr;
  u32 b13[6], b14[6], bspsr[6]; u32 fiq_hi[5], usr_hi[5];
  u32 pc; /* address of next instruction */
  int halted;
  u8 bios[0x4000]; u8 ewram[0x40000]; u8 iwram[0x8000]; u8 io[0x400];
  u8 pal[0x400]; u8 vram[0x18000]; u8 oam[0x400]; u8 sram[0x10000];
  struct { u32 isrc, idst, icnt; int active; } dma[4];
  struct { u32 counter, reload, ctrl, sub; } tm[4];
  int fifo[2];
  int line; u64 frame; u64 cycles_total; s64 budget;
  s32 bgx[2], bgy[2];
  u16 keys; /* active-high pressed */
  u32 fb[240*160];
  u64 insn_count;
  u32 last_hle;
} GBA;

static GBA G;
static u8* rom; static u32 romsize; static u32 rommask;
static int trace_on; static u32 brk_pc=0xFFFFFFFF; static int brk_hit; static u32 key_reads, key_pc;
u32 gba_key_pc(void){return key_pc;}
u32 gba_key_reads(void){return key_reads;}

static inline u16 IO16(u32 a){ return (u16)(G.io[a]|(G.io[a+1]<<8)); }
static inline void IO16W(u32 a,u16 v){ G.io[a]=(u8)v; G.io[a+1]=(u8)(v>>8); }
static inline u32 IO32(u32 a){ return IO16(a)|((u32)IO16(a+2)<<16); }

/* ---------------- mode banking ---------------- */
static int bank_index(u32 m){ switch(m&0x1F){case M_FIQ:return 1;case M_IRQ:return 2;case M_SVC:return 3;case M_ABT:return 4;case M_UND:return 5;default:return 0;} }
static void switch_mode(u32 newcpsr){
  u32 om=G.cpsr&0x1F, nm=newcpsr&0x1F;
  if(om!=nm){
    int oi=bank_index(om), ni=bank_index(nm);
    G.b13[oi]=G.r[13]; G.b14[oi]=G.r[14]; if(oi) G.bspsr[oi]=G.spsr;
    if(om==M_FIQ){ for(int i=0;i<5;i++){G.fiq_hi[i]=G.r[8+i]; G.r[8+i]=G.usr_hi[i];} }
    if(nm==M_FIQ){ for(int i=0;i<5;i++){G.usr_hi[i]=G.r[8+i]; G.r[8+i]=G.fiq_hi[i];} }
    G.r[13]=G.b13[ni]; G.r[14]=G.b14[ni]; G.spsr = ni? G.bspsr[ni] : G.cpsr;
  }
  G.cpsr=newcpsr;
}

/* ---------------- memory ---------------- */
static void dma_check_start(int ch);
static void io_write16(u32 a,u16 v);
static u32 timer_read(int i);

static u16 io_read16(u32 a){
  a&=0x3FE;
  switch(a){
    case 0x004:{ u16 v=IO16(4)&0xFF38; if(G.line>=160&&G.line<227) v|=1; /* hblank flag maintained in io */ v|=G.io[0x3FF]&2; if(G.line==(IO16(4)>>8)) v|=4; return v; }
    case 0x006: return (u16)G.line;
    case 0x100: return (u16)timer_read(0);
    case 0x104: return (u16)timer_read(1);
    case 0x108: return (u16)timer_read(2);
    case 0x10C: return (u16)timer_read(3);
    case 0x130: key_reads++; key_pc=G.pc; return (u16)(~G.keys & 0x3FF);
    case 0x3FE: return 0;
  }
  return IO16(a);
}
static u8 rd8(u32 a){
  switch(a>>24){
    case 0x00: return a<0x4000? G.bios[a]:0;
    case 0x02: return G.ewram[a&0x3FFFF];
    case 0x03: return G.iwram[a&0x7FFF];
    case 0x04: if((a&0xFFFFFF)<0x400){u16 v=io_read16(a&0x3FE); return (a&1)?(u8)(v>>8):(u8)v;} return 0;
    case 0x05: return G.pal[a&0x3FF];
    case 0x06: { u32 o=a&0x1FFFF; if(o>=0x18000) o-=0x8000; return G.vram[o]; }
    case 0x07: return G.oam[a&0x3FF];
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: { u32 o=a&0x1FFFFFF; if(o<romsize) return rom[o]; return (u8)((a>>1)>>((a&1)*8)); }
    case 0x0E: case 0x0F: return G.sram[a&0xFFFF];
  }
  return 0;
}
static u16 rd16(u32 a){
  a&=~1u;
  switch(a>>24){
    case 0x00: return a<0x4000? (u16)(G.bios[a]|(G.bios[a+1]<<8)):0;
    case 0x02: { u32 o=a&0x3FFFF; return (u16)(G.ewram[o]|(G.ewram[o+1]<<8)); }
    case 0x03: { u32 o=a&0x7FFF; return (u16)(G.iwram[o]|(G.iwram[o+1]<<8)); }
    case 0x04: if((a&0xFFFFFF)<0x400) return io_read16(a&0x3FE); return 0;
    case 0x05: { u32 o=a&0x3FF; return (u16)(G.pal[o]|(G.pal[o+1]<<8)); }
    case 0x06: { u32 o=a&0x1FFFF; if(o>=0x18000) o-=0x8000; return (u16)(G.vram[o]|(G.vram[o+1]<<8)); }
    case 0x07: { u32 o=a&0x3FF; return (u16)(G.oam[o]|(G.oam[o+1]<<8)); }
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: { u32 o=a&0x1FFFFFF; if(o+1<romsize) return (u16)(rom[o]|(rom[o+1]<<8)); return (u16)(a>>1); }
    case 0x0E: case 0x0F: { u8 b=G.sram[a&0xFFFF]; return (u16)(b*0x101u); }
  }
  return 0;
}
static u32 rd32(u32 a){
  a&=~3u;
  switch(a>>24){
    case 0x02: { u32 o=a&0x3FFFF; u32 v; memcpy(&v,G.ewram+o,4); return v; }
    case 0x03: { u32 o=a&0x7FFF; u32 v; memcpy(&v,G.iwram+o,4); return v; }
    case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: { u32 o=a&0x1FFFFFF; if(o+3<romsize){u32 v; memcpy(&v,rom+o,4); return v;} return ((a>>1)&0xFFFF)|((((a+2)>>1)&0xFFFF)<<16); }
    case 0x0E: case 0x0F: { u8 b=G.sram[a&0xFFFF]; return b*0x01010101u; }
  }
  return rd16(a)|((u32)rd16(a+2)<<16);
}
static void wr16(u32 a,u16 v);
static void wr8(u32 a,u8 v){
  switch(a>>24){
    case 0x02: G.ewram[a&0x3FFFF]=v; return;
    case 0x03: G.iwram[a&0x7FFF]=v; return;
    case 0x04: { u32 o=a&0xFFFFFF; if(o>=0x400) return;
      if(o==0x301){ if(!(v&0x80)) G.halted=1; return; }
      if(o==0x202||o==0x203){ G.io[o]&=(u8)~v; return; }
      u16 cur=IO16(o&~1u); if(o&1) cur=(u16)((cur&0xFF)|(v<<8)); else cur=(u16)((cur&0xFF00)|v);
      io_write16(o&~1u,cur); return; }
    case 0x05: { u32 o=a&0x3FE; G.pal[o]=v; G.pal[o+1]=v; return; }
    case 0x06: { u32 o=a&0x1FFFE; if(o>=0x18000) o-=0x8000; u32 dc=IO16(0)&7; u32 lim=dc>=3?0x14000:0x10000; if(o<lim){G.vram[o]=v;G.vram[o+1]=v;} return; }
    case 0x07: return;
    case 0x0E: case 0x0F: G.sram[a&0xFFFF]=v; return;
  }
}
static void wr16(u32 a,u16 v){
  a&=~1u;
  switch(a>>24){
    case 0x02: { u32 o=a&0x3FFFF; G.ewram[o]=(u8)v; G.ewram[o+1]=(u8)(v>>8); return; }
    case 0x03: { u32 o=a&0x7FFF; G.iwram[o]=(u8)v; G.iwram[o+1]=(u8)(v>>8); return; }
    case 0x04: { u32 o=a&0xFFFFFF; if(o<0x400) io_write16(o,v); return; }
    case 0x05: { u32 o=a&0x3FF; G.pal[o]=(u8)v; G.pal[o+1]=(u8)(v>>8); return; }
    case 0x06: { u32 o=a&0x1FFFF; if(o>=0x18000) o-=0x8000; G.vram[o]=(u8)v; G.vram[o+1]=(u8)(v>>8); return; }
    case 0x07: { u32 o=a&0x3FF; G.oam[o]=(u8)v; G.oam[o+1]=(u8)(v>>8); return; }
    case 0x0E: case 0x0F: G.sram[a&0xFFFF]=(u8)v; return;
  }
}
static void wr32(u32 a,u32 v){
  a&=~3u;
  switch(a>>24){
    case 0x02: memcpy(G.ewram+(a&0x3FFFF),&v,4); return;
    case 0x03: memcpy(G.iwram+(a&0x7FFF),&v,4); return;
    case 0x0E: case 0x0F: G.sram[a&0xFFFF]=(u8)v; return;
  }
  wr16(a,(u16)v); wr16(a+2,(u16)(v>>16));
}

/* ---------------- timers ---------------- */
static const u32 tm_shift[4]={0,6,8,10};
static u32 timer_read(int i){ return G.tm[i].counter&0xFFFF; }
static void fifo_tick(int fifo);
static void timer_overflow(int i){
  if(G.tm[i].ctrl&0x40){ G.io[0x202]|=(u8)(1u<<(3+i)); }
  /* sound fifo */
  u16 sh=IO16(0x82);
  if(i<2){
    if(((sh>>10)&1)==(u32)i) fifo_tick(0);
    if(((sh>>14)&1)==(u32)i) fifo_tick(1);
  }
  if(i<3 && (G.tm[i+1].ctrl&0x80) && (G.tm[i+1].ctrl&4)){
    if(++G.tm[i+1].counter>0xFFFF){ G.tm[i+1].counter=G.tm[i+1].reload; timer_overflow(i+1); }
  }
}
static void timers_tick(u32 cyc){
  for(int i=0;i<4;i++){
    u32 c=G.tm[i].ctrl; if(!(c&0x80)) continue; if(c&4) continue;
    u32 sh=tm_shift[c&3];
    G.tm[i].sub+=cyc; u32 inc=G.tm[i].sub>>sh; G.tm[i].sub&=(1u<<sh)-1;
    while(inc){
      u32 room=0x10000-G.tm[i].counter;
      if(inc<room){ G.tm[i].counter+=inc; break; }
      inc-=room; G.tm[i].counter=G.tm[i].reload; timer_overflow(i);
    }
  }
}

/* ---------------- DMA ---------------- */
static const u32 dma_base[4]={0xB0,0xBC,0xC8,0xD4};
static void dma_run(int ch){
  u32 b=dma_base[ch]; u16 cnt=IO16(b+10);
  int w32=(cnt>>10)&1; int dm=(cnt>>5)&3, sm=(cnt>>7)&3; int timing=(cnt>>12)&3;
  u32 n=G.dma[ch].icnt;
  int fifo = (timing==3 && (ch==1||ch==2));
  if(fifo){ n=4; w32=1; dm=2; }
  u32 src=G.dma[ch].isrc, dst=G.dma[ch].idst; int sz=w32?4:2;
  int sstep = sm==0?sz: sm==1?-sz: 0; int dstep = dm==0||dm==3?sz: dm==1?-sz:0;
  for(u32 i=0;i<n;i++){
    if(w32) wr32(dst, rd32(src)); else wr16(dst, rd16(src));
    src+=sstep; dst+=dstep;
  }
  G.budget -= (s64)n*2;
  G.dma[ch].isrc=src; if(!fifo) G.dma[ch].idst=dst;
  if(cnt&0x4000) G.io[0x202+((8+ch)>>3)] |= (u8)(1u<<((8+ch)&7));
  if((cnt&0x200) && timing!=0){
    u32 c=IO16(b+8); if(ch==3){ if(!c) c=0x10000; } else { c&=0x3FFF; if(!c) c=0x4000; }
    G.dma[ch].icnt=c;
    if(dm==3) G.dma[ch].idst=IO32(b+4)&(ch==3?0x0FFFFFFF:0x07FFFFFF);
  } else {
    cnt&=0x7FFF; IO16W(b+10,cnt); G.dma[ch].active=0;
  }
}
static void dma_check_start(int ch){ (void)ch; }
static void dma_trigger(int timing){
  for(int ch=0;ch<4;ch++){
    u16 cnt=IO16(dma_base[ch]+10);
    if((cnt&0x8000) && ((cnt>>12)&3)==timing) dma_run(ch);
  }
}
static void fifo_tick(int f){
  if(G.fifo[f]>0) G.fifo[f]--;
  if(G.fifo[f]<=16){
    u32 fa = f? 0x040000A4 : 0x040000A0;
    for(int ch=1;ch<=2;ch++){
      u16 cnt=IO16(dma_base[ch]+10);
      if((cnt&0x8000) && ((cnt>>12)&3)==3 && (G.dma[ch].idst&~3u)==fa){ dma_run(ch); break; }
    }
    G.fifo[f]+=16;
  }
}

/* ---------------- IO write ---------------- */
static void io_write16(u32 a,u16 v){
  a&=0x3FE;
  switch(a){
    case 0x004: v=(u16)((IO16(4)&7)|(v&0xFFF8)); IO16W(4,v); return;
    case 0x006: return;
    case 0x028: case 0x02A: IO16W(a,v); G.bgx[0]=((s32)(IO32(0x28)<<4))>>4; return;
    case 0x02C: case 0x02E: IO16W(a,v); G.bgy[0]=((s32)(IO32(0x2C)<<4))>>4; return;
    case 0x038: case 0x03A: IO16W(a,v); G.bgx[1]=((s32)(IO32(0x38)<<4))>>4; return;
    case 0x03C: case 0x03E: IO16W(a,v); G.bgy[1]=((s32)(IO32(0x3C)<<4))>>4; return;
    case 0x0A0: case 0x0A2: case 0x0A4: case 0x0A6: return;
    case 0x0BA: case 0x0C6: case 0x0D2: case 0x0DE: {
      int ch=(a-0xBA)/12; u16 old=IO16(a); IO16W(a,v);
      if(!(old&0x8000) && (v&0x8000)){
        u32 b=dma_base[ch];
        G.dma[ch].isrc=IO32(b)&(ch==0?0x07FFFFFF:0x0FFFFFFF);
        G.dma[ch].idst=IO32(b+4)&(ch==3?0x0FFFFFFF:0x07FFFFFF);
        u32 c=IO16(b+8); if(ch==3){ if(!c) c=0x10000; } else { c&=0x3FFF; if(!c) c=0x4000; }
        G.dma[ch].icnt=c; G.dma[ch].active=1;
        if(((v>>12)&3)==0) dma_run(ch);
      }
      return; }
    case 0x100: case 0x104: case 0x108: case 0x10C: G.tm[(a-0x100)>>2].reload=v; IO16W(a,v); return;
    case 0x102: case 0x106: case 0x10A: case 0x10E: { int i=(a-0x102)>>2; u32 old=G.tm[i].ctrl; G.tm[i].ctrl=v; IO16W(a,v);
      if(!(old&0x80)&&(v&0x80)){ G.tm[i].counter=G.tm[i].reload; G.tm[i].sub=0; } return; }
    case 0x0A8: return;
    case 0x084: IO16W(a,(u16)((v&0x80)|(IO16(a)&0xF))); return;
    case 0x082: if(v&0x0800) G.fifo[0]=0; if(v&0x8000) G.fifo[1]=0; IO16W(a,(u16)(v&0x77FF)); return;
    case 0x130: return;
    case 0x202: G.io[0x202]&=(u8)~v; G.io[0x203]&=(u8)~(v>>8); return;
    case 0x300: G.io[0x300]=(u8)v; if(!(v&0x8000)) G.halted=1; return;
  }
  IO16W(a,v);
}

/* ---------------- HLE SWI ---------------- */
static s16 ArcTan(s32 i,s32*r1,s32*r3){
  s32 a=-((i*i)>>14); s32 b=((0xA9*a)>>14)+0x390; b=((b*a)>>14)+0x91C; b=((b*a)>>14)+0xFB6; b=((b*a)>>14)+0x16AA; b=((b*a)>>14)+0x2081; b=((b*a)>>14)+0x3651; b=((b*a)>>14)+0xA2F9;
  if(r1)*r1=a; if(r3)*r3=b; return (s16)((i*b)>>16);
}
static s32 ArcTan2(s32 x,s32 y,s32*r1,s32*r3){
  if(!y){ return x>=0?0:0x8000; }
  if(!x){ return y>=0?0x4000:0xC000; }
  if(y>=0){ if(x>=0){ if(x>=y) return ArcTan((y<<14)/x,r1,r3); } else if(-x>=y) return ArcTan((y<<14)/x,r1,r3)+0x8000; return 0x4000-ArcTan((x<<14)/y,r1,r3); }
  else { if(x<=0){ if(-x>-y) return ArcTan((y<<14)/x,r1,r3)+0x8000; } else if(x>=-y) return ArcTan((y<<14)/x,r1,r3)+0x10000; return 0xC000-ArcTan((x<<14)/y,r1,r3); }
}
static void lz77(u32 src,u32 dst,int vram){
  u32 hdr=rd32(src); u32 len=hdr>>8; src+=4; u32 written=0; u16 hw=0; int hb=0;
  #define OUTB(b) do{ if(vram){ if(hb){hw|=(u16)((b)<<8); wr16(dst+written-1,hw); hb=0;} else {hw=(b); hb=1;} } else wr8(dst+written,(b)); written++; }while(0)
  while(written<len){
    u8 flags=rd8(src++);
    for(int i=0;i<8&&written<len;i++){
      if(flags&0x80){ u8 b1=rd8(src++), b2=rd8(src++); u32 n=(b1>>4)+3; u32 disp=(((b1&0xF)<<8)|b2)+1;
        for(u32 k=0;k<n&&written<len;k++){ u8 c; if(vram){ u32 p=dst+written-disp; if(hb && disp==1) c=(u8)hw; else { c=rd8(p); if(((written-disp)>>1)==((written)>>1) && hb) c=(u8)hw; } } else c=rd8(dst+written-disp); OUTB(c);} }
      else { u8 c=rd8(src++); OUTB(c); }
      flags<<=1;
    }
  }
  if(vram&&hb) wr16(dst+written-1,hw);
  #undef OUTB
}
static void rlunc(u32 src,u32 dst,int vram){
  u32 len=rd32(src)>>8; src+=4; u32 w=0; u16 hw=0; int hb=0;
  while(w<len){ u8 f=rd8(src++); if(f&0x80){ u32 n=(f&0x7F)+3; u8 c=rd8(src++); for(u32 k=0;k<n&&w<len;k++){ if(vram){ if(hb){hw|=(u16)(c<<8);wr16(dst+w-1,hw);hb=0;}else{hw=c;hb=1;} } else wr8(dst+w,c); w++; } }
    else { u32 n=(f&0x7F)+1; for(u32 k=0;k<n&&w<len;k++){ u8 c=rd8(src++); if(vram){ if(hb){hw|=(u16)(c<<8);wr16(dst+w-1,hw);hb=0;}else{hw=c;hb=1;} } else wr8(dst+w,c); w++; } } }
}
static void hle_swi(u32 n){
  G.last_hle=n;
  u32* r=G.r;
  switch(n){
    case 0x00: /* SoftReset */ {
      u8 flag=rd8(0x03007FFA); memset(G.iwram+0x7E00,0,0x200);
      switch_mode((G.cpsr&~0x1Fu)|M_SVC); G.r[13]=0x03007FE0; switch_mode((G.cpsr&~0x1Fu)|M_IRQ); G.r[13]=0x03007FA0; switch_mode(M_SYS); G.r[13]=0x03007F00;
      for(int i=0;i<13;i++) G.r[i]=0; G.r[14]=0;
      G.pc = flag? 0x02000000:0x08000000; G.cpsr=M_SYS; return; }
    case 0x01: { u32 f=r[0];
      if(f&1) memset(G.ewram,0,sizeof G.ewram);
      if(f&2) memset(G.iwram,0,0x7E00);
      if(f&4) memset(G.pal,0,sizeof G.pal);
      if(f&8) memset(G.vram,0,sizeof G.vram);
      if(f&16) memset(G.oam,0,sizeof G.oam);
      return; }
    case 0x06: case 0x07: { s32 a=(s32)r[0], b=(s32)r[1]; if(n==7){s32 t=a;a=b;b=t;} if(!b){ r[0]=a<0?-1:1; r[1]=a; r[3]=1; return;} s32 q=a/b, m=a%b; r[0]=(u32)q; r[1]=(u32)m; r[3]=(u32)(q<0?-q:q); return; }
    case 0x08: { u32 x=r[0]; u32 res=0, bit=1u<<30; while(bit>x) bit>>=2; while(bit){ if(x>=res+bit){ x-=res+bit; res=(res>>1)+bit;} else res>>=1; bit>>=2; } r[0]=res; return; }
    case 0x09: { s32 r1,r3; r[0]=(u32)(s32)ArcTan((s32)(s16)r[0],&r1,&r3); r[1]=(u32)r1; r[3]=(u32)r3; return; }
    case 0x0A: { s32 r1=r[1],r3=r[3]; r[0]=(u32)(ArcTan2((s32)(s16)r[0],(s32)(s16)r[1],&r1,&r3)&0xFFFF); r[1]=(u32)r1; r[3]=(u32)r3; return; }
    case 0x0B: { u32 s=r[0],d=r[1],c=r[2]; u32 cnt=c&0x1FFFFF; int fill=(c>>24)&1; int w=(c>>26)&1;
      if(w){ s&=~3u; d&=~3u; u32 v=rd32(s); for(u32 i=0;i<cnt;i++){ if(!fill) v=rd32(s+i*4); wr32(d+i*4,v);} }
      else { s&=~1u; d&=~1u; u16 v=rd16(s); for(u32 i=0;i<cnt;i++){ if(!fill) v=rd16(s+i*2); wr16(d+i*2,v);} }
      return; }
    case 0x0C: { u32 s=r[0]&~3u,d=r[1]&~3u,c=r[2]; u32 cnt=(c&0x1FFFFF); cnt=(cnt+7)&~7u; int fill=(c>>24)&1; u32 v=rd32(s);
      for(u32 i=0;i<cnt;i++){ if(!fill) v=rd32(s+i*4); wr32(d+i*4,v);} return; }
    case 0x0E: { u32 s=r[0], d=r[1]; for(u32 i=0;i<r[2];i++){
        s32 ox=(s32)rd32(s), oy=(s32)rd32(s+4); s16 cx=(s16)rd16(s+8), cy=(s16)rd16(s+10); s16 sx=(s16)rd16(s+12), sy=(s16)rd16(s+14); u16 th=rd16(s+16)>>8; s+=20;
        double a=th*2*3.14159265358979/256.0; s32 cs=(s32)(256.0*__builtin_cos(a)), sn=(s32)(256.0*__builtin_sin(a));
        s16 pa=(s16)((sx*cs)>>8), pb=(s16)((-sx*sn)>>8), pc=(s16)((sy*sn)>>8), pd=(s16)((sy*cs)>>8);
        wr16(d,(u16)pa); wr16(d+2,(u16)pb); wr16(d+4,(u16)pc); wr16(d+6,(u16)pd);
        wr32(d+8,(u32)(ox-(pa*cx+pb*cy))); wr32(d+12,(u32)(oy-(pc*cx+pd*cy))); d+=16; } return; }
    case 0x0F: { u32 s=r[0], d=r[1], off=r[3]; for(u32 i=0;i<r[2];i++){
        s16 sx=(s16)rd16(s), sy=(s16)rd16(s+2); u16 th=rd16(s+4)>>8; s+=8;
        double a=th*2*3.14159265358979/256.0; s32 cs=(s32)(256.0*__builtin_cos(a)), sn=(s32)(256.0*__builtin_sin(a));
        wr16(d,(u16)((sx*cs)>>8)); wr16(d+off,(u16)((-sx*sn)>>8)); wr16(d+off*2,(u16)((sy*sn)>>8)); wr16(d+off*3,(u16)((sy*cs)>>8)); d+=off*4; } return; }
    case 0x10: { u32 s=r[0], d=r[1], info=r[2]; u16 len=rd16(info); u8 sw=rd8(info+2), dw=rd8(info+3); u32 ofs=rd32(info+4); int zf=ofs>>31; ofs&=0x7FFFFFFF;
      u32 out=0; int ob=0; for(u32 i=0;i<len;i++){ u8 b=rd8(s+i); for(int k=0;k<8;k+=sw){ u32 v=(b>>k)&((1u<<sw)-1); if(v||zf) v+=ofs; out|=v<<ob; ob+=dw; if(ob>=32){ wr32(d,out); d+=4; out=0; ob=0; } } } return; }
    case 0x11: lz77(r[0],r[1],0); return;
    case 0x12: lz77(r[0],r[1],1); return;
    case 0x14: rlunc(r[0],r[1],0); return;
    case 0x15: rlunc(r[0],r[1],1); return;
    case 0x16: case 0x17: case 0x18: { u32 s=r[0], d=r[1]; u32 hdr=rd32(s); u32 len=hdr>>8; s+=4; if(n==0x18){ u16 acc=0; for(u32 i=0;i<len;i+=2){ acc+=rd16(s+i); wr16(d+i,acc);} } else { u8 acc=0; u16 hw=0; for(u32 i=0;i<len;i++){ acc+=rd8(s+i); if(n==0x16) wr8(d+i,acc); else { if(i&1){hw|=(u16)(acc<<8); wr16(d+i-1,hw);} else hw=acc; } } } return; }
    case 0x19: return;
    case 0x1F: { /* MidiKey2Freq */ return; }
    default: return;
  }
}

/* ---------------- CPU ---------------- */
static inline int cond_ok(u32 c){
  u32 f=G.cpsr; int N=!!(f&FN), Z=!!(f&FZ), C=!!(f&FC), V=!!(f&FV);
  switch(c){case 0:return Z;case 1:return !Z;case 2:return C;case 3:return !C;case 4:return N;case 5:return !N;case 6:return V;case 7:return !V;
  case 8:return C&&!Z;case 9:return !C||Z;case 10:return N==V;case 11:return N!=V;case 12:return !Z&&N==V;case 13:return Z||N!=V;case 14:return 1;}
  return 0;
}
static inline void setNZ(u32 v){ G.cpsr=(G.cpsr&~(FN|FZ))|(v&FN)|(v?0:FZ); }
static inline void setC(int c){ if(c) G.cpsr|=FC; else G.cpsr&=~FC; }
static inline void setV(int v){ if(v) G.cpsr|=FV; else G.cpsr&=~FV; }
static inline u32 add_flags(u32 a,u32 b,u32 cin,int s){ u64 r=(u64)a+b+cin; u32 r32=(u32)r; if(s){ setNZ(r32); setC((int)(r>>32)); setV((int)((~(a^b)&(a^r32))>>31)); } return r32; }
static inline u32 sub_flags(u32 a,u32 b,u32 cin,int s){ /* a - b - !cin */ u64 r=(u64)a+(u64)(~b)+cin; u32 r32=(u32)r; if(s){ setNZ(r32); setC((int)(r>>32)); setV((int)(((a^b)&(a^r32))>>31)); } return r32; }

static u32 shift_op(u32 v,int type,u32 amt,int reg,int* carry){
  int C=!!(G.cpsr&FC); *carry=C;
  if(reg){ amt&=0xFF; if(!amt) return v;
    switch(type){
      case 0: if(amt<32){*carry=(v>>(32-amt))&1; return v<<amt;} if(amt==32){*carry=v&1; return 0;} *carry=0; return 0;
      case 1: if(amt<32){*carry=(v>>(amt-1))&1; return v>>amt;} if(amt==32){*carry=v>>31; return 0;} *carry=0; return 0;
      case 2: if(amt<32){*carry=(v>>(amt-1))&1; return (u32)((s32)v>>amt);} *carry=v>>31; return (v>>31)?0xFFFFFFFFu:0;
      case 3: { u32 a=amt&31; if(!a){ *carry=v>>31; return v;} *carry=(v>>(a-1))&1; return (v>>a)|(v<<(32-a)); }
    }
  } else {
    switch(type){
      case 0: if(!amt) return v; *carry=(v>>(32-amt))&1; return v<<amt;
      case 1: if(!amt){ *carry=v>>31; return 0;} *carry=(v>>(amt-1))&1; return v>>amt;
      case 2: if(!amt){ *carry=v>>31; return (v>>31)?0xFFFFFFFFu:0;} *carry=(v>>(amt-1))&1; return (u32)((s32)v>>amt);
      case 3: if(!amt){ *carry=v&1; return (v>>1)|((u32)C<<31);} *carry=(v>>(amt-1))&1; return (v>>amt)|(v<<(32-amt));
    }
  }
  return v;
}

static inline int region_cost(u32 a,int w32){
  switch(a>>24){ case 0x02: return w32?6:3; case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: return w32?6:4; case 0x0E: case 0x0F: return 5; case 0x06: return w32?2:1; default: return 1; }
}

static void branch_to(u32 v){ if(G.cpsr&FT) G.pc=v&~1u; else G.pc=v&~3u; }
static void cpsr_restore(void){ u32 s=G.spsr; switch_mode(s); }

static u32 rdload32(u32 a){ u32 v=rd32(a); u32 rot=(a&3)*8; return rot? (v>>rot)|(v<<(32-rot)) : v; }

static int arm_exec(u32 op){
  u32 cur=G.pc; G.pc=cur+4; G.r[15]=cur+8;
  int cyc=1;
  if(!cond_ok(op>>28)) return 1;
  u32 bits=(op>>25)&7;
  switch(bits){
  case 0: case 1: {
    if(bits==0){
      if((op&0x0FFFFFF0)==0x012FFF10){ u32 v=G.r[op&15]; if(v&1){ G.cpsr|=FT; G.pc=v&~1u; } else G.pc=v&~3u; return 3; }
      if((op&0x0FC000F0)==0x00000090){ /* MUL/MLA */
        u32 rd=(op>>16)&15, rn=(op>>12)&15, rs=(op>>8)&15, rm=op&15; u32 v=G.r[rm]*G.r[rs]; if(op&(1<<21)) v+=G.r[rn]; G.r[rd]=v; if(op&(1<<20)) setNZ(v); return 4; }
      if((op&0x0F8000F0)==0x00800090){ /* long mul */
        u32 hi=(op>>16)&15, lo=(op>>12)&15, rs=(op>>8)&15, rm=op&15; u64 res;
        if(op&(1<<22)) res=(u64)((s64)(s32)G.r[rm]*(s64)(s32)G.r[rs]); else res=(u64)G.r[rm]*(u64)G.r[rs];
        if(op&(1<<21)) res+=((u64)G.r[hi]<<32)|G.r[lo];
        G.r[lo]=(u32)res; G.r[hi]=(u32)(res>>32);
        if(op&(1<<20)){ G.cpsr=(G.cpsr&~(FN|FZ))|((res>>63)?FN:0)|(res?0:FZ); }
        return 5; }
      if((op&0x0FB00FF0)==0x01000090){ /* SWP */
        u32 rn=(op>>16)&15, rd=(op>>12)&15, rm=op&15; u32 a=G.r[rn];
        if(op&(1<<22)){ u8 t=rd8(a); wr8(a,(u8)G.r[rm]); G.r[rd]=t; } else { u32 t=rdload32(a); wr32(a,G.r[rm]); G.r[rd]=t; }
        return 4; }
      if((op&0x0E000090)==0x00000090 && (op&0x60)){ /* halfword transfers */
        u32 rn=(op>>16)&15, rd=(op>>12)&15; int P=(op>>24)&1, U=(op>>23)&1, I=(op>>22)&1, W=(op>>21)&1, L=(op>>20)&1; u32 sh=(op>>5)&3;
        u32 off = I? (((op>>4)&0xF0)|(op&0xF)) : G.r[op&15];
        u32 base=G.r[rn]; u32 addr = P? (U?base+off:base-off) : base;
        cyc=1+region_cost(addr,0);
        if(L){ u32 v;
          if(sh==1){ v=rd16(addr); if(addr&1) v=(v>>8)|(v<<24); }
          else if(sh==2){ v=(u32)(s32)(int8_t)rd8(addr); }
          else { if(addr&1) v=(u32)(s32)(int8_t)rd8(addr); else v=(u32)(s32)(int16_t)rd16(addr); }
          if(!P){ G.r[rn]= U?base+off:base-off; } else if(W) G.r[rn]=addr;
          if(rd==15) branch_to(v); else G.r[rd]=v;
        } else {
          u32 v=G.r[rd]; if(rd==15) v=cur+12;
          if(sh==1) wr16(addr,(u16)v);
          if(!P){ G.r[rn]= U?base+off:base-off; } else if(W) G.r[rn]=addr;
        }
        return cyc; }
      if((op&0x0FBF0FFF)==0x010F0000){ G.r[(op>>12)&15]= (op&(1<<22))? G.spsr : G.cpsr; return 1; }
    }
    if((op&0x0DB0F000)==0x0120F000){ /* MSR */
      u32 v; if(op&(1<<25)){ u32 imm=op&0xFF, rot=((op>>8)&15)*2; v= rot? (imm>>rot)|(imm<<(32-rot)) : imm; } else v=G.r[op&15];
      u32 mask=0; if(op&(1<<19)) mask|=0xFF000000; if(op&(1<<18)) mask|=0x00FF0000; if(op&(1<<17)) mask|=0x0000FF00; if(op&(1<<16)) mask|=0x000000FF;
      if(op&(1<<22)){ if((G.cpsr&0x1F)!=M_USR && (G.cpsr&0x1F)!=M_SYS) G.spsr=(G.spsr&~mask)|(v&mask); }
      else { if((G.cpsr&0x1F)==M_USR) mask&=0xFF000000; u32 nc=(G.cpsr&~mask)|(v&mask); nc|=0x10; switch_mode(nc); }
      return 1; }
    /* data processing */
    u32 opc=(op>>21)&15; int S=(op>>20)&1; u32 rn=(op>>16)&15, rd=(op>>12)&15; u32 op2; int carry;
    u32 a;
    if(bits==1){ u32 imm=op&0xFF, rot=((op>>8)&15)*2; op2= rot? (imm>>rot)|(imm<<(32-rot)) : imm; carry= rot? (int)(op2>>31) : !!(G.cpsr&FC); a=G.r[rn]; }
    else {
      int type=(op>>5)&3; u32 rm=op&15;
      if(op&0x10){ G.r[15]=cur+12; u32 amt=G.r[(op>>8)&15]&0xFF; op2=shift_op(G.r[rm],type,amt,1,&carry); a=G.r[rn]; G.r[15]=cur+8; cyc++; }
      else { op2=shift_op(G.r[rm],type,(op>>7)&31,0,&carry); a=G.r[rn]; }
    }
    u32 res=0; int wr=1; int Cin=!!(G.cpsr&FC);
    int sflags = S && rd!=15;
    switch(opc){
      case 0: res=a&op2; if(sflags){setNZ(res);setC(carry);} break;
      case 1: res=a^op2; if(sflags){setNZ(res);setC(carry);} break;
      case 2: res=sub_flags(a,op2,1,sflags); break;
      case 3: res=sub_flags(op2,a,1,sflags); break;
      case 4: res=add_flags(a,op2,0,sflags); break;
      case 5: res=add_flags(a,op2,(u32)Cin,sflags); break;
      case 6: res=sub_flags(a,op2,(u32)Cin,sflags); break;
      case 7: res=sub_flags(op2,a,(u32)Cin,sflags); break;
      case 8: res=a&op2; wr=0; setNZ(res); setC(carry); break;
      case 9: res=a^op2; wr=0; setNZ(res); setC(carry); break;
      case 10: sub_flags(a,op2,1,1); wr=0; break;
      case 11: add_flags(a,op2,0,1); wr=0; break;
      case 12: res=a|op2; if(sflags){setNZ(res);setC(carry);} break;
      case 13: res=op2; if(sflags){setNZ(res);setC(carry);} break;
      case 14: res=a&~op2; if(sflags){setNZ(res);setC(carry);} break;
      case 15: res=~op2; if(sflags){setNZ(res);setC(carry);} break;
    }
    if(wr){
      if(rd==15){ if(S){ cpsr_restore(); } branch_to(res); cyc+=2; }
      else G.r[rd]=res;
    } else if(rd==15 && S){ /* TSTP etc - ignore */ }
    return cyc; }
  case 2: case 3: {
    if(bits==3 && (op&0x10)){
      if(op==0xE7FFDEFE){ hle_swi(G.r[12]); return 10; }
      return 1; /* undefined */
    }
    u32 rn=(op>>16)&15, rd=(op>>12)&15; int P=(op>>24)&1, U=(op>>23)&1, B=(op>>22)&1, W=(op>>21)&1, L=(op>>20)&1;
    u32 off; if(bits==2) off=op&0xFFF; else { int c; off=shift_op(G.r[op&15],(op>>5)&3,(op>>7)&31,0,&c); }
    u32 base=G.r[rn]; u32 addr= P? (U?base+off:base-off) : base; u32 nb = U?base+off:base-off;
    cyc=1+region_cost(addr,!B);
    if(L){ u32 v= B? rd8(addr) : rdload32(addr);
      if(!P) G.r[rn]=nb; else if(W) G.r[rn]=addr;
      if(rd==15){ branch_to(v); cyc+=2; } else G.r[rd]=v;
    } else { u32 v=G.r[rd]; if(rd==15) v=cur+12;
      if(B) wr8(addr,(u8)v); else wr32(addr,v);
      if(!P) G.r[rn]=nb; else if(W) G.r[rn]=addr;
    }
    return cyc; }
  case 4: { /* LDM/STM */
    u32 rn=(op>>16)&15; int P=(op>>24)&1, U=(op>>23)&1, S=(op>>22)&1, W=(op>>21)&1, L=(op>>20)&1; u32 list=op&0xFFFF;
    int n=__builtin_popcount(list); if(!n){ list=0x8000; n=16; /* quirk approximated */ }
    u32 base=G.r[rn]; u32 start; u32 nb;
    if(U){ start= P? base+4 : base; nb=base+4u*(u32)n; } else { start = P? base-4u*(u32)n : base-4u*(u32)n+4; nb=base-4u*(u32)n; }
    if(list==0x8000 && !(op&0xFFFF)) nb = U? base+0x40: base-0x40;
    int usr = S && !(L && (list&0x8000));
    u32 oldmode=G.cpsr;
    if(usr && (G.cpsr&0x1F)!=M_USR && (G.cpsr&0x1F)!=M_SYS){ switch_mode((G.cpsr&~0x1Fu)|M_SYS); }
    u32 addr=start; cyc=1+n*region_cost(start,1)/2+1;
    if(L){
      if(W) G.r[rn]=nb;
      for(int i=0;i<16;i++) if(list&(1u<<i)){ u32 v=rd32(addr); addr+=4; if(i==15){ if(S){ cpsr_restore(); } branch_to(v); cyc+=2; } else G.r[i]=v; }
    } else {
      int first=1;
      for(int i=0;i<16;i++) if(list&(1u<<i)){ u32 v=G.r[i]; if(i==15) v=cur+12; if(i==(int)rn && !first) v=nb; wr32(addr,v); addr+=4; first=0; }
      if(W) G.r[rn]=nb;
    }
    if(usr && (oldmode&0x1F)!=M_USR && (oldmode&0x1F)!=M_SYS) switch_mode(oldmode);
    return cyc; }
  case 5: { s32 off=((s32)(op<<8))>>6; if(op&(1<<24)) G.r[14]=cur+4; G.pc=cur+8+(u32)off; return 3; }
  case 6: return 1;
  case 7: if(op&(1<<24)){ /* SWI */
      u32 c=G.cpsr; switch_mode((c&~0x3Fu)|M_SVC|FI); G.spsr=c; G.r[14]=cur+4; G.pc=0x08; return 3; }
    return 1;
  }
  return 1;
}

static int thumb_exec(u16 op){
  u32 cur=G.pc; G.pc=cur+2; G.r[15]=cur+4; u32* r=G.r; int carry;
  switch(op>>13){
  case 0: {
    if(((op>>11)&3)==3){ /* add/sub */
      u32 rd=op&7, rs=(op>>3)&7; u32 v= (op&0x400)? (u32)((op>>6)&7) : r[(op>>6)&7];
      if(op&0x200) r[rd]=sub_flags(r[rs],v,1,1); else r[rd]=add_flags(r[rs],v,0,1); return 1; }
    u32 rd=op&7, rs=(op>>3)&7, amt=(op>>6)&31; u32 v=shift_op(r[rs],(op>>11)&3,amt,0,&carry); r[rd]=v; setNZ(v); setC(carry); return 1; }
  case 1: { u32 rd=(op>>8)&7, imm=op&0xFF;
    switch((op>>11)&3){ case 0: r[rd]=imm; setNZ(imm); break; case 1: sub_flags(r[rd],imm,1,1); break; case 2: r[rd]=add_flags(r[rd],imm,0,1); break; case 3: r[rd]=sub_flags(r[rd],imm,1,1); break; }
    return 1; }
  case 2: {
    if((op>>10)==0x10){ /* ALU */
      u32 rd=op&7, rs=(op>>3)&7; u32 a=r[rd], b=r[rs]; u32 res;
      switch((op>>6)&15){
        case 0: res=a&b; r[rd]=res; setNZ(res); break;
        case 1: res=a^b; r[rd]=res; setNZ(res); break;
        case 2: res=shift_op(a,0,b,1,&carry); r[rd]=res; setNZ(res); setC(carry); return 2;
        case 3: res=shift_op(a,1,b,1,&carry); r[rd]=res; setNZ(res); setC(carry); return 2;
        case 4: res=shift_op(a,2,b,1,&carry); r[rd]=res; setNZ(res); setC(carry); return 2;
        case 5: r[rd]=add_flags(a,b,!!(G.cpsr&FC),1); break;
        case 6: r[rd]=sub_flags(a,b,!!(G.cpsr&FC),1); break;
        case 7: res=shift_op(a,3,b,1,&carry); r[rd]=res; setNZ(res); setC(carry); return 2;
        case 8: setNZ(a&b); break;
        case 9: r[rd]=sub_flags(0,b,1,1); break;
        case 10: sub_flags(a,b,1,1); break;
        case 11: add_flags(a,b,0,1); break;
        case 12: res=a|b; r[rd]=res; setNZ(res); break;
        case 13: res=a*b; r[rd]=res; setNZ(res); return 4;
        case 14: res=a&~b; r[rd]=res; setNZ(res); break;
        case 15: res=~b; r[rd]=res; setNZ(res); break;
      }
      return 1; }
    if((op>>10)==0x11){ /* hi reg */
      u32 rd=(op&7)|((op>>4)&8), rs=(op>>3)&15; u32 v=r[rs];
      switch((op>>8)&3){
        case 0: v=r[rd]+v; if(rd==15) branch_to(v); else r[rd]=v; return rd==15?3:1;
        case 1: sub_flags(r[rd],v,1,1); return 1;
        case 2: if(rd==15) branch_to(v); else r[rd]=v; return rd==15?3:1;
        case 3: if(v&1){ G.pc=v&~1u; } else { G.cpsr&=~FT; G.pc=v&~3u; } return 3;
      }
    }
    if((op>>11)==9){ u32 rd=(op>>8)&7; u32 a=((cur+4)&~3u)+((op&0xFF)<<2); r[rd]=rd32(a); return 1+region_cost(a,1); }
    /* 0101 load/store reg offset */
    { u32 rd=op&7, rb=(op>>3)&7, ro=(op>>6)&7; u32 a=r[rb]+r[ro]; int c=1+region_cost(a,0);
      switch((op>>9)&7){
        case 0: wr32(a,r[rd]); break;
        case 1: wr16(a,(u16)r[rd]); break;
        case 2: wr8(a,(u8)r[rd]); break;
        case 3: r[rd]=(u32)(s32)(int8_t)rd8(a); break;
        case 4: r[rd]=rdload32(a); break;
        case 5: { u32 v=rd16(a); if(a&1) v=(v>>8)|(v<<24); r[rd]=v; } break;
        case 6: r[rd]=rd8(a); break;
        case 7: r[rd]= (a&1)? (u32)(s32)(int8_t)rd8(a) : (u32)(s32)(int16_t)rd16(a); break;
      }
      return c; }
  }
  case 3: { u32 rd=op&7, rb=(op>>3)&7, off=(op>>6)&31; int B=(op>>12)&1, L=(op>>11)&1;
    u32 a = r[rb] + (B? off : off*4); int c=1+region_cost(a,!B);
    if(L) r[rd]= B? rd8(a) : rdload32(a); else { if(B) wr8(a,(u8)r[rd]); else wr32(a,r[rd]); }
    return c; }
  case 4: {
    if(!(op&0x1000)){ u32 rd=op&7, rb=(op>>3)&7; u32 a=r[rb]+((op>>6)&31)*2; if(op&0x800){ u32 v=rd16(a); if(a&1) v=(v>>8)|(v<<24); r[rd]=v; } else wr16(a,(u16)r[rd]); return 1+region_cost(a,0); }
    { u32 rd=(op>>8)&7; u32 a=r[13]+((op&0xFF)<<2); if(op&0x800) r[rd]=rdload32(a); else wr32(a,r[rd]); return 1+region_cost(a,1); } }
  case 5: {
    if(!(op&0x1000)){ u32 rd=(op>>8)&7; u32 imm=(op&0xFF)<<2; r[rd]= (op&0x800)? r[13]+imm : ((cur+4)&~3u)+imm; return 1; }
    if((op&0xFF00)==0xB000){ u32 imm=(op&0x7F)<<2; if(op&0x80) r[13]-=imm; else r[13]+=imm; return 1; }
    if((op&0x0600)==0x0400){ /* push/pop */
      int L=(op>>11)&1, R=(op>>8)&1; u32 list=op&0xFF; int c=1;
      if(L){ u32 a=r[13]; for(int i=0;i<8;i++) if(list&(1u<<i)){ r[i]=rd32(a); a+=4; c+=region_cost(a,1); } if(R){ u32 v=rd32(a); a+=4; G.pc=v&~1u; c+=2; } r[13]=a; }
      else { int n=__builtin_popcount(list)+R; u32 a=r[13]-4u*(u32)n; r[13]=a; for(int i=0;i<8;i++) if(list&(1u<<i)){ wr32(a,r[i]); a+=4; c+=region_cost(a,1);} if(R) wr32(a,r[14]); }
      return c; }
    return 1; }
  case 6: {
    if(!(op&0x1000)){ /* ldmia/stmia */
      u32 rb=(op>>8)&7; u32 list=op&0xFF; u32 a=r[rb]; int c=1;
      if(!list){ return 1; }
      if(op&0x800){ for(int i=0;i<8;i++) if(list&(1u<<i)){ r[i]=rd32(a); a+=4; c+=region_cost(a,1);} if(!(list&(1u<<rb))) r[rb]=a; }
      else { u32 nb=r[rb]+4u*(u32)__builtin_popcount(list); int first=1; for(int i=0;i<8;i++) if(list&(1u<<i)){ u32 v=r[i]; if(i==(int)rb && !first) v=nb; wr32(a,v); a+=4; first=0; c+=region_cost(a,1);} r[rb]=a; }
      return c; }
    u32 cond=(op>>8)&15;
    if(cond==15){ u32 c=G.cpsr; switch_mode((c&~0x3Fu)|M_SVC|FI); G.spsr=c; G.r[14]=cur+2; G.pc=0x08; return 3; }
    if(cond==14) return 1;
    if(cond_ok(cond)){ s32 off=((s32)(int8_t)(op&0xFF))*2; G.pc=cur+4+(u32)off; return 3; }
    return 1; }
  case 7: {
    u32 h=(op>>11)&3;
    if(h==0){ s32 off=((s32)((u32)op<<21))>>20; G.pc=cur+4+(u32)off; return 3; }
    if(h==2){ s32 off=((s32)((u32)op<<21))>>9; r[14]=cur+4+(u32)off; return 1; }
    if(h==3){ u32 t=r[14]+((op&0x7FF)<<1); r[14]=(cur+2)|1; G.pc=t&~1u; return 3; }
    return 1; }
  }
  return 1;
}

static void take_irq(void){
  u32 c=G.cpsr; u32 ret=G.pc+4;
  switch_mode((c&~0x3Fu)|M_IRQ|FI); G.spsr=c; G.r[14]=ret; G.pc=0x18;
}

#define PCRING 64
static u32 pcring[PCRING]; static int pcri;
static int prof_on; static u32 prof_hist[(0x01800000u)>>4]; static u32 prof_iw[0x8000>>4]; static u32 prof_other;
void gba_prof(int on){ prof_on=on; if(on==2){ memset(prof_hist,0,sizeof prof_hist); memset(prof_iw,0,sizeof prof_iw); prof_other=0; prof_on=1; } }
u32* gba_prof_hist(void){ return prof_hist; } u32* gba_prof_iw(void){ return prof_iw; } u32 gba_prof_other(void){ return prof_other; }
static void cpu_run(s64 cycles){
  G.budget+=cycles;
  while(G.budget>0){
    u16 ie=IO16(0x200), iflag=IO16(0x202);
    if(G.halted){ if(ie&iflag) G.halted=0; else { timers_tick((u32)G.budget); G.budget=0; break; } }
    if((ie&iflag) && (G.io[0x208]&1) && !(G.cpsr&FI)) take_irq();
    int c;
    u32 pc=G.pc;
    if(pc==brk_pc){ brk_hit=1; return; } pcring[pcri++&(PCRING-1)]=pc|(G.cpsr&FT?1:0);
    if(G.cpsr&FT){ u16 op=rd16(pc); c=thumb_exec(op); int rc=region_cost(pc,0); c+= (rc>1)?1:0; }
    else { u32 op=rd32(pc); c=arm_exec(op); int rc=region_cost(pc,1); c+= (rc>1)?2:0; }
    G.insn_count++;
    if(trace_on>0){ trace_on--; fprintf(stderr,"%08x %s cpsr=%08x r0=%08x r1=%08x r2=%08x r3=%08x r12=%08x sp=%08x lr=%08x\n",pc,(G.cpsr&FT)?"T":"A",G.cpsr,G.r[0],G.r[1],G.r[2],G.r[3],G.r[12],G.r[13],G.r[14]); }
    if(prof_on && pc>=0x08800000u && pc<0x0A000000u) prof_hist[(pc-0x08800000u)>>4]+=(u32)c; else if(prof_on && pc>=0x03000000u && pc<0x03008000u) prof_iw[(pc-0x03000000u)>>4]+=(u32)c; else if(prof_on) prof_other+=(u32)c;
    G.budget-=c; timers_tick((u32)c);
  }
}

/* ---------------- PPU ---------------- */
#define TRANSP 0x8000
static u16 bgline[4][240];
static u16 objline[240]; static u8 objprio[240]; static u8 objsemi[240]; static u8 objwin[240];
static inline u16 palc(u32 i){ return (u16)(G.pal[i*2]|(G.pal[i*2+1]<<8))&0x7FFF; }
static void render_text_bg(int n,int y){
  u16 cnt=IO16(8+n*2); u32 cbb=((cnt>>2)&3)*0x4000u, sbb=((cnt>>8)&0x1F)*0x800u; int c256=(cnt>>7)&1, size=cnt>>14;
  u32 hofs=IO16(0x10+n*4)&0x1FF, vofs=IO16(0x12+n*4)&0x1FF;
  int W= (size&1)?512:256, H=(size&2)?512:256;
  u32 yy=((u32)y+vofs)&(u32)(H-1);
  int mos = (cnt>>6)&1; if(mos){ u32 mv=((IO16(0x4C)>>4)&15)+1; yy = (((u32)y/mv)*mv+vofs)&(u32)(H-1); }
  for(int x=0;x<240;x++){
    u32 px=((u32)x+hofs)&(u32)(W-1); u32 tx=px>>3, ty=yy>>3; u32 sb=sbb;
    if(tx>=32){ sb+=0x800; tx-=32; } if(ty>=32){ sb+= (size==3)?0x1000:0x800; ty-=32; }
    u32 ea=sb+(ty*32+tx)*2; u16 e=(u16)(G.vram[ea]|(G.vram[ea+1]<<8));
    u32 tile=e&0x3FF; u32 ix=px&7, iy=yy&7; if(e&0x400) ix=7-ix; if(e&0x800) iy=7-iy;
    u16 col=TRANSP;
    if(c256){ u32 a=cbb+tile*64+iy*8+ix; if(a<0x10000){ u8 ci=G.vram[a]; if(ci) col=palc(ci);} }
    else { u32 a=cbb+tile*32+iy*4+(ix>>1); if(a<0x10000){ u8 b=G.vram[a]; u8 ci=(ix&1)?(b>>4):(b&15); if(ci) col=palc((e>>12)*16+ci);} }
    bgline[n][x]=col;
  }
}
static void render_affine_bg(int n,int y){
  (void)y; u16 cnt=IO16(8+n*2); u32 cbb=((cnt>>2)&3)*0x4000u, sbb=((cnt>>8)&0x1F)*0x800u; int size=128<<(cnt>>14); int wrap=(cnt>>13)&1;
  int k=n-2; s16 pa=(s16)IO16(0x20+k*16), pc=(s16)IO16(0x24+k*16); s32 rx=G.bgx[k], ry=G.bgy[k];
  for(int x=0;x<240;x++){
    s32 tx=rx>>8, ty=ry>>8; rx+=pa; ry+=pc; u16 col=TRANSP;
    if(wrap){ tx&=size-1; ty&=size-1; }
    if(tx>=0&&ty>=0&&tx<size&&ty<size){ u8 t=G.vram[sbb+(u32)(ty>>3)*(u32)(size>>3)+(u32)(tx>>3)]; u32 a=cbb+t*64u+(u32)(ty&7)*8+(u32)(tx&7); if(a<0x10000){ u8 ci=G.vram[a]; if(ci) col=palc(ci);} }
    bgline[n][x]=col;
  }
}
static void render_bitmap_bg(int mode,int y){
  (void)y; s16 pa=(s16)IO16(0x20), pc=(s16)IO16(0x24); s32 rx=G.bgx[0], ry=G.bgy[0]; u16 dc=IO16(0); u32 page=(dc&0x10)?0xA000:0;
  for(int x=0;x<240;x++){
    s32 tx=rx>>8, ty=ry>>8; rx+=pa; ry+=pc; u16 col=TRANSP;
    if(mode==3){ if(tx>=0&&ty>=0&&tx<240&&ty<160){ u32 a=((u32)ty*240+(u32)tx)*2; col=(u16)(G.vram[a]|(G.vram[a+1]<<8))&0x7FFF; } }
    else if(mode==4){ if(tx>=0&&ty>=0&&tx<240&&ty<160){ u8 ci=G.vram[page+(u32)ty*240+(u32)tx]; if(ci) col=palc(ci);} }
    else { if(tx>=0&&ty>=0&&tx<160&&ty<128){ u32 a=page+((u32)ty*160+(u32)tx)*2; col=(u16)(G.vram[a]|(G.vram[a+1]<<8))&0x7FFF; } }
    bgline[2][x]=col;
  }
}
static const u8 objw[3][4]={{8,16,32,64},{16,32,32,64},{8,8,16,32}};
static const u8 objh[3][4]={{8,16,32,64},{8,8,16,32},{16,32,32,64}};
static void render_obj(int y){
  for(int x=0;x<240;x++){ objline[x]=TRANSP; objprio[x]=4; objsemi[x]=0; objwin[x]=0; }
  u16 dc=IO16(0); if(!(dc&0x1000)) return; int map1d=(dc>>6)&1; int bitmap=(dc&7)>=3;
  for(int i=0;i<128;i++){
    u16 a0=(u16)(G.oam[i*8]|(G.oam[i*8+1]<<8)), a1=(u16)(G.oam[i*8+2]|(G.oam[i*8+3]<<8)), a2=(u16)(G.oam[i*8+4]|(G.oam[i*8+5]<<8));
    int aff=(a0>>8)&1; if(!aff && (a0&0x200)) continue;
    int shape=a0>>14; if(shape==3) continue; int sz=a1>>14; int w=objw[shape][sz], h=objh[shape][sz];
    int dbl = aff && (a0&0x200); int bw= dbl? w*2:w, bh= dbl? h*2:h;
    int oy=a0&0xFF; if(oy+bh>256) oy-=256; /* wrap */
    if(y<oy || y>=oy+bh) continue;
    int ox=a1&0x1FF; if(ox&0x100) ox-=512;
    int mode=(a0>>10)&3; if(mode==3) continue;
    int c256=(a0>>13)&1; u32 tile=a2&0x3FF; int prio=(a2>>10)&3; u32 pb=a2>>12;
    if(bitmap && tile<512) continue;
    s16 PA=256,PB=0,PC=0,PD=256;
    if(aff){ u32 g=(a1>>9)&31; PA=(s16)(G.oam[g*32+6]|(G.oam[g*32+7]<<8)); PB=(s16)(G.oam[g*32+14]|(G.oam[g*32+15]<<8)); PC=(s16)(G.oam[g*32+22]|(G.oam[g*32+23]<<8)); PD=(s16)(G.oam[g*32+30]|(G.oam[g*32+31]<<8)); }
    int ly=y-oy;
    for(int bx=0;bx<bw;bx++){
      int sx=ox+bx; if(sx<0||sx>=240) continue;
      int tx,ty;
      if(aff){ int cx=bx-bw/2, cy=ly-bh/2; tx=((PA*cx+PB*cy)>>8)+w/2; ty=((PC*cx+PD*cy)>>8)+h/2; if(tx<0||ty<0||tx>=w||ty>=h) continue; }
      else { tx=bx; ty=ly; if(a1&0x1000) tx=w-1-tx; if(a1&0x2000) ty=h-1-ty; }
      u32 toff;
      if(c256){ u32 t= map1d? tile+(u32)(ty>>3)*(u32)(w>>3)*2+(u32)(tx>>3)*2 : tile+(u32)(ty>>3)*32+(u32)(tx>>3)*2; toff=0x10000+(t&0x3FF)*32+(u32)(ty&7)*8+(u32)(tx&7); }
      else { u32 t= map1d? tile+(u32)(ty>>3)*(u32)(w>>3)+(u32)(tx>>3) : tile+(u32)(ty>>3)*32+(u32)(tx>>3); toff=0x10000+(t&0x3FF)*32+(u32)(ty&7)*4+(u32)((tx&7)>>1); }
      if(toff>=0x18000) continue;
      u8 ci; if(c256) ci=G.vram[toff]; else { u8 b=G.vram[toff]; ci=(tx&1)?(b>>4):(b&15); }
      if(!ci) continue;
      if(mode==2){ objwin[sx]=1; continue; }
      if(prio<objprio[sx]){ objprio[sx]=(u8)prio; objline[sx]= c256? palc(256+ci) : palc(256+pb*16+ci); objsemi[sx]=(mode==1); }
    }
  }
}
static inline u16 blend(u16 a,u16 b,int eva,int evb){
  int r=((a&31)*eva+(b&31)*evb)>>4, g=(((a>>5)&31)*eva+((b>>5)&31)*evb)>>4, bl=(((a>>10)&31)*eva+((b>>10)&31)*evb)>>4;
  if(r>31)r=31; if(g>31)g=31; if(bl>31)bl=31; return (u16)(r|(g<<5)|(bl<<10));
}
static void render_line(int y){
  u16 dc=IO16(0); u32* out=G.fb+y*240;
  if(dc&0x80){ for(int x=0;x<240;x++) out[x]=0xFFFFFF; return; }
  int mode=dc&7; int en[4]={0,0,0,0};
  switch(mode){
    case 0: for(int i=0;i<4;i++) if(dc&(0x100<<i)){ en[i]=1; render_text_bg(i,y);} break;
    case 1: for(int i=0;i<2;i++) if(dc&(0x100<<i)){ en[i]=1; render_text_bg(i,y);} if(dc&0x400){ en[2]=1; render_affine_bg(2,y);} break;
    case 2: for(int i=2;i<4;i++) if(dc&(0x100<<i)){ en[i]=1; render_affine_bg(i,y);} break;
    case 3: case 4: case 5: if(dc&0x400){ en[2]=1; render_bitmap_bg(mode,y);} break;
  }
  render_obj(y);
  int prio[4]; for(int i=0;i<4;i++) prio[i]=IO16(8+i*2)&3;
  u16 winin=IO16(0x48), winout=IO16(0x4A); int anywin=(dc>>13)&7;
  u16 bld=IO16(0x50), bal=IO16(0x52); int eva=bal&31, evb=(bal>>8)&31; if(eva>16)eva=16; if(evb>16)evb=16; int evy=IO16(0x54)&31; if(evy>16)evy=16;
  int bmode=(bld>>6)&3; u16 back=palc(0);
  int w0in=0,w1in=0;
  if(dc&0x2000){ u16 h=IO16(0x40), v=IO16(0x44); int y1=v>>8,y2=v&0xFF; w0in = (y1<=y2)? (y>=y1&&y<y2) : (y>=y1||y<y2); (void)h; }
  if(dc&0x4000){ u16 v=IO16(0x46); int y1=v>>8,y2=v&0xFF; w1in = (y1<=y2)? (y>=y1&&y<y2) : (y>=y1||y<y2); }
  for(int x=0;x<240;x++){
    int mask=0x3F;
    if(anywin){
      int inw0=0,inw1=0;
      if(w0in){ u16 h=IO16(0x40); int x1=h>>8,x2=h&0xFF; inw0=(x1<=x2)?(x>=x1&&x<x2):(x>=x1||x<x2); }
      if(w1in){ u16 h=IO16(0x42); int x1=h>>8,x2=h&0xFF; inw1=(x1<=x2)?(x>=x1&&x<x2):(x>=x1||x<x2); }
      if(inw0) mask=winin&0x3F; else if(inw1) mask=(winin>>8)&0x3F; else if((dc&0x8000)&&objwin[x]) mask=(winout>>8)&0x3F; else mask=winout&0x3F;
    }
    int top=5, sec=5; u16 tc=back, sc=back;
    int found=0;
    for(int p=0;p<4&&found<2;p++){
      if((mask&0x10) && objline[x]!=TRANSP && objprio[x]==p){ if(!found){top=4;tc=objline[x];} else {sec=4;sc=objline[x];} found++; if(found==2)break; }
      for(int i=0;i<4&&found<2;i++){ if(en[i] && prio[i]==p && (mask&(1<<i)) && bgline[i][x]!=TRANSP){ if(!found){top=i;tc=bgline[i][x];} else {sec=i;sc=bgline[i][x];} found++; } }
    }
    u16 c=tc;
    if(top==4 && objsemi[x] && (bld&(0x100<<sec))) c=blend(tc,sc,eva,evb);
    else if((mask&0x20) && (bld&(1<<top))){
      if(bmode==1 && (bld&(0x100<<sec))) c=blend(tc,sc,eva,evb);
      else if(bmode==2){ int r=tc&31,g=(tc>>5)&31,b=(tc>>10)&31; r+=((31-r)*evy)>>4; g+=((31-g)*evy)>>4; b+=((31-b)*evy)>>4; c=(u16)(r|(g<<5)|(b<<10)); }
      else if(bmode==3){ int r=tc&31,g=(tc>>5)&31,b=(tc>>10)&31; r-=(r*evy)>>4; g-=(g*evy)>>4; b-=(b*evy)>>4; c=(u16)(r|(g<<5)|(b<<10)); }
    }
    u32 R=(c&31)<<3, Gc=((c>>5)&31)<<3, B=((c>>10)&31)<<3; out[x]=(R|(R>>5))<<16|(Gc|(Gc>>5))<<8|(B|(B>>5));
  }
}

/* ---------------- frame ---------------- */
static void run_frame(void){
  for(int ln=0;ln<228;ln++){
    G.line=ln;
    u16 ds=IO16(4);
    if(ln==160){ G.bgx[0]=((s32)(IO32(0x28)<<4))>>4; G.bgy[0]=((s32)(IO32(0x2C)<<4))>>4; G.bgx[1]=((s32)(IO32(0x38)<<4))>>4; G.bgy[1]=((s32)(IO32(0x3C)<<4))>>4;
      if(ds&8) G.io[0x202]|=1; dma_trigger(1); }
    if((ds>>8)==ln && (ds&0x20)) G.io[0x202]|=4;
    G.io[0x3FF]&=~2;
    cpu_run(960); if(brk_hit) return;
    if(ln<160){ render_line(ln); G.bgx[0]+=(s16)IO16(0x22); G.bgy[0]+=(s16)IO16(0x26); G.bgx[1]+=(s16)IO16(0x32); G.bgy[1]+=(s16)IO16(0x36); }
    G.io[0x3FF]|=2;
    if(ds&0x10) G.io[0x202]|=2;
    if(ln<160) dma_trigger(2);
    cpu_run(272); if(brk_hit) return;
  }
  G.frame++;
}

/* ---------------- API ---------------- */
int gba_load(const char* path){
  FILE* f=fopen(path,"rb"); if(!f) return -1; fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  free(rom); rom=malloc((size_t)n+4); if(fread(rom,1,(size_t)n,f)!=(size_t)n){fclose(f);return -2;} fclose(f); romsize=(u32)n; rommask=0; return 0;
}
void gba_reset(u32 entry){
  memset(&G,0,sizeof G); memset(G.sram,0xFF,sizeof G.sram);
  memcpy(G.bios,bios_bin,sizeof bios_bin);
  G.cpsr=M_SYS;
  for(int i=0;i<6;i++){ G.b13[i]=0x03007F00; }
  G.b13[2]=0x03007FA0; G.b13[3]=0x03007FE0;
  G.r[13]=0x03007F00; G.pc=entry? entry:0x08000000;
  IO16W(0x20,0x100); IO16W(0x26,0x100); IO16W(0x30,0x100); IO16W(0x36,0x100);
  IO16W(0x88,0x200);
}
void gba_run_frames(int n){ for(int i=0;i<n;i++) run_frame(); }
void gba_set_keys(u32 k){ G.keys=(u16)k; }
u32* gba_fb(void){ return G.fb; }
u32 gba_rd8(u32 a){ return rd8(a);} u32 gba_rd16(u32 a){ return rd16(a);} u32 gba_rd32(u32 a){ return rd32(a);}
void gba_wr8(u32 a,u32 v){ wr8(a,(u8)v);} void gba_wr16(u32 a,u32 v){ wr16(a,(u16)v);} void gba_wr32(u32 a,u32 v){ wr32(a,v);}
u32 gba_reg(int i){ if(i==15) return G.pc; if(i==16) return G.cpsr; if(i==17) return G.spsr; if(i>=18&&i<24) return G.b14[i-18]; if(i>=24&&i<30) return G.b13[i-24]; return G.r[i]; }
u64 gba_frame(void){ return G.frame; }
u64 gba_insns(void){ return G.insn_count; }
int gba_halted(void){ return G.halted; }
u8* gba_mem(int which){ switch(which){case 0:return G.ewram;case 1:return G.iwram;case 2:return G.io;case 3:return G.pal;case 4:return G.vram;case 5:return G.oam;case 6:return G.sram;} return 0; }
int gba_save_state(const char* p){ FILE* f=fopen(p,"wb"); if(!f) return -1; fwrite(&G,sizeof G,1,f); fclose(f); return 0; }
int gba_load_state(const char* p){ FILE* f=fopen(p,"rb"); if(!f) return -1; size_t k=fread(&G,sizeof G,1,f); fclose(f); return k==1?0:-2; }
void gba_trace(int n){ trace_on=n; }
u32 gba_pcring(int i){ return pcring[(pcri-1-i)&(PCRING-1)]; }
/* run until pc hits addr (or frames exhausted). returns 1 if hit. */
int gba_run_until_pc(u32 addr,int maxframes){
  for(int f=0;f<maxframes;f++){
    for(int ln=0;ln<228;ln++){ /* coarse */ }
    /* simple: step frames checking pc ring */
    run_frame();
    for(int i=0;i<PCRING;i++) if((pcring[i]&~1u)==addr) return 1;
  }
  return 0;
}
void gba_set_break(u32 pc){ brk_pc=pc; brk_hit=0; }
int gba_break_hit(void){ return brk_hit; }
void gba_set_reg(int i,u32 v){ if(i==15) G.pc=v; else G.r[i]=v; }
