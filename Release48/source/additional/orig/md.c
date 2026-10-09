/* Experimental Mega Drive text-adventure runtime, no sound or persistent saves yet. */
#include <stdint.h>
#include <string.h>
#include "m68k.h"
#include "pager.h"
#define R16(a) (*(volatile uint16_t*)(a))
#define R32(a) (*(volatile uint32_t*)(a))
typedef struct {u8 ram[65536],vram[65536],zram[8192];u16 cram[64],vsram[40];u8 reg[24];int pending;u16 first;u32 addr;int code,fill_pending;u16 status;int line;u64 frame;int vint_pending,hint_pending,hcounter;u8 pad_th;u16 pad;int sram_on;u8 ctrl1;} MD;
static MD M;static uint32_t romsize;
void *memset(void*p,int c,size_t n){u8*d=p;while(n--)*d++=c;return p;}
void *memcpy(void*p,const void*s,size_t n){u8*d=p;const u8*q=s;while(n--)*d++=*q++;return p;}
void __aeabi_memcpy(void*p,const void*s,size_t n){memcpy(p,s,n);}void __aeabi_memcpy4(void*p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memclr(void*p,size_t n){memset(p,0,n);}void __aeabi_memclr4(void*p,size_t n){memset(p,0,n);}
unsigned __aeabi_uidiv(unsigned n,unsigned d){unsigned q=0,b=1;if(!d)return 0;while(d<=n&&!(d&0x80000000)){d<<=1;b<<=1;}while(b){if(n>=d){n-=d;q|=b;}d>>=1;b>>=1;}return q;}
int __aeabi_idiv(int n,int d){unsigned un=n<0?0u-(unsigned)n:(unsigned)n,ud=d<0?0u-(unsigned)d:(unsigned)d;unsigned q=__aeabi_uidiv(un,ud);return (n<0)^(d<0)?0u-q:q;}
/* ---------------- VDP ---------------- */
static u16 vdp_status(void){
  u16 s=0x3400|0x200; if(M.line>=224) s|=8; if(M.vint_pending) s|=0x80; return s;
}
static void vdp_data_write(u16 v);
static void vdp_dma(void){
  int mode=M.reg[23]>>6; u32 len=((u32)M.reg[20]<<8)|M.reg[19]; if(!len) len=0x10000;
  if(mode<=1){
    u32 src=(((u32)M.reg[23]&0x7F)<<17)|((u32)M.reg[22]<<9)|((u32)M.reg[21]<<1);
    for(u32 i=0;i<len;i++){ u16 w=m68k_read16(src); src+=2; if((src&0xFFFF)==0 && 0) src-=0x20000; vdp_data_write(w); }
    M.reg[19]=M.reg[20]=0;
  } else if(mode==2){ M.fill_pending=1; }
  else { /* copy */
    u32 src=((u32)M.reg[22]<<8)|M.reg[21];
    for(u32 i=0;i<len;i++){ M.vram[M.addr&0xFFFF]=M.vram[src&0xFFFF]; src++; M.addr+=M.reg[15]; }
  }
}
static void vdp_data_write(u16 v){
  if(M.fill_pending){
    M.fill_pending=0; u32 len=((u32)M.reg[20]<<8)|M.reg[19]; if(!len) len=0x10000;
    if((M.code&0xF)==1){ M.vram[M.addr&0xFFFE]=(u8)(v>>8); M.vram[(M.addr&0xFFFE)+1]=(u8)v;
      for(u32 i=0;i<len;i++){ M.vram[(M.addr^1)&0xFFFF]=(u8)(v>>8); M.addr+=M.reg[15]; } }
    else { for(u32 i=0;i<len;i++){ if((M.code&0xF)==3) M.cram[(M.addr>>1)&63]=v; else if((M.addr>>1)<40) M.vsram[M.addr>>1]=v; M.addr+=M.reg[15]; } }
    return;
  }
  switch(M.code&0xF){
    case 1: { u32 a=M.addr&0xFFFF; if(a&1){ M.vram[a&0xFFFE]=(u8)v; M.vram[a|1]=(u8)(v>>8);} else { M.vram[a]=(u8)(v>>8); M.vram[a+1]=(u8)v; } break; }
    case 3: M.cram[(M.addr>>1)&63]=v&0xEEE; break;
    case 5: if(((M.addr>>1)&63)<40) M.vsram[(M.addr>>1)&63]=v&0x7FF; break;
  }
  M.addr+=M.reg[15];
}
static u16 vdp_data_read(void){
  u16 v=0;
  switch(M.code&0xF){ case 0: v=(u16)((M.vram[M.addr&0xFFFE]<<8)|M.vram[(M.addr&0xFFFE)+1]); break; case 8: v=M.cram[(M.addr>>1)&63]; break; case 4: v=((M.addr>>1)&63)<40?M.vsram[(M.addr>>1)&63]:0; break; }
  M.addr+=M.reg[15]; return v;
}
static void vdp_ctrl_write(u16 v){
  if(M.pending){
    M.pending=0; M.addr=(M.first&0x3FFF)|((u32)(v&3)<<14); M.code=((M.first>>14)&3)|((v>>2)&0x3C);
    if((M.code&0x20) && (M.reg[1]&0x10)) vdp_dma();
    return;
  }
  if((v&0xC000)==0x8000){ int r=(v>>8)&0x1F; if(r<24) M.reg[r]=(u8)v; M.code=0; return; }
  M.first=v; M.pending=1; M.addr=(M.addr&0xC000)|(v&0x3FFF); M.code=(M.code&0x3C)|((v>>14)&3);
}

/* ---------------- bus ---------------- */
static u8 io_read(u32 a){
  a&=0x1F;
  if(a==0x01) return 0xA0;
  if(a==0x03){ /* port 1 */
    u8 th=M.pad_th&0x40; u16 p=M.pad; u8 r;
    if(th){ r=0x40|0x3F; if(p&1) r&=~1; if(p&2) r&=~2; if(p&4) r&=~4; if(p&8) r&=~8; if(p&16) r&=~0x10; if(p&32) r&=~0x20; }
    else { r=0x33; if(p&1) r&=~1; if(p&2) r&=~2; if(p&64) r&=~0x10; if(p&128) r&=~0x20; }
    return r|0x80;
  }
  if(a==0x05||a==0x07) return 0x7F;
  if(a==0x09) return M.ctrl1;
  return 0;
}
u8 m68k_read8(u32 a){
  a&=0xFFFFFF;
  if(a<romsize)return pager_read(a);
  if(a>=0xE00000) return M.ram[a&0xFFFF];
  if(a>=0xA04000&&a<0xA06000) return 0;
  if(a>=0xA00000&&a<0xA10000) return M.zram[a&0x1FFF];
  if(a>=0xA10000&&a<0xA10020) return io_read(a);
  if(a==0xA11100||a==0xA11101) return 0x00;
  if(a>=0xC00000&&a<0xC00020){ u16 w=m68k_read16(a&~1u); return (a&1)?(u8)w:(u8)(w>>8); }
  return 0xFF;
}
u16 m68k_read16(u32 a){
  a&=0xFFFFFE;
  if(a<romsize)return (pager_read(a)<<8)|pager_read(a+1);
  if(a>=0xE00000){ u32 o=a&0xFFFF; return (u16)((M.ram[o]<<8)|M.ram[o+1]); }
  if(a>=0xC00000&&a<0xC00020){
    u32 p=a&0x1F;
    if(p<4) return vdp_data_read();
    if(p<8){ M.pending=0; u16 s=vdp_status(); return s; }
    if(p<0xC){ int h=M.hcounter; return (u16)(((M.line>0xEA?M.line-6:M.line)&0xFF)<<8|(h&0xFF)); }
    return 0;
  }
  return (u16)((m68k_read8(a)<<8)|m68k_read8(a+1));
}
u32 m68k_read32(u32 a){ return ((u32)m68k_read16(a)<<16)|m68k_read16(a+2); }
void m68k_write8(u32 a,u8 v){
  a&=0xFFFFFF;
  if(a>=0xE00000){  M.ram[a&0xFFFF]=v; return; }
  
  if(a==0xA130F1){ M.sram_on=v&1; return; }
  if(a>=0xA00000&&a<0xA10000){ M.zram[a&0x1FFF]=v; return; }
  if(a>=0xA10000&&a<0xA10020){ u32 r=a&0x1F; if(r==0x03) M.pad_th=v; if(r==0x09) M.ctrl1=v; return; }
  if(a>=0xC00000&&a<0xC00020){ m68k_write16(a&~1u,(u16)(v|(v<<8))); return; }
}
void m68k_write16(u32 a,u16 v){
  a&=0xFFFFFE;
  if(a>=0xE00000){  u32 o=a&0xFFFF; M.ram[o]=(u8)(v>>8); M.ram[o+1]=(u8)v; return; }
  if(a>=0xC00000&&a<0xC00020){ u32 p=a&0x1F; if(p<4) vdp_data_write(v); else if(p<8) vdp_ctrl_write(v); return; }
  m68k_write8(a,(u8)(v>>8)); m68k_write8(a+1,(u8)v);
}
void m68k_write32(u32 a,u32 v){ m68k_write16(a,(u16)(v>>16)); m68k_write16(a+2,(u16)v); }

/* ---------------- renderer ---------------- */
static inline u32 cram_rgb(u16 c){ u32 r=((c>>1)&7)*36, g=((c>>5)&7)*36, b=((c>>9)&7)*36; return (r<<16)|(g<<8)|b; }
static u8 linebuf[3][320]; /* palette index + prio<<7 for B, A, sprite; 0 = transparent */
static int plane_w(void){ int s=M.reg[16]&3; return s==0?32:s==1?64:128; }
static int plane_h(void){ int s=(M.reg[16]>>4)&3; return s==0?32:s==1?64:128; }
static void render_plane(int which,int y,u8* out){
  u32 base= which==0? ((u32)(M.reg[2]&0x38)<<10) : ((u32)(M.reg[4]&7)<<13);
  int pw=plane_w(), ph=plane_h();
  u32 hs=((u32)(M.reg[13]&0x3F))<<10; int hmode=M.reg[11]&3; int vmode=(M.reg[11]>>2)&1;
  int hline= hmode==0?0: hmode==2? (y&~7) : y;
  u32 ha=hs+(u32)hline*4+(which==0?0:2); int hsc=(int)(((M.vram[ha&0xFFFF]<<8)|M.vram[(ha+1)&0xFFFF])&0x3FF);
  for(int x=0;x<320;x++){
    int col=x>>4; int vs= vmode? M.vsram[(col*2+(which==0?0:1))%40] : M.vsram[which==0?0:1];
    int py=(y+vs)&(ph*8-1); int px=(x-hsc)&(pw*8-1);
    u32 ea=base+(u32)(((py>>3)*pw+(px>>3))*2); u16 e=(u16)((M.vram[ea&0xFFFF]<<8)|M.vram[(ea+1)&0xFFFF]);
    int tx=px&7, ty=py&7; if(e&0x800) tx=7-tx; if(e&0x1000) ty=7-ty;
    u32 ta=((u32)(e&0x7FF)<<5)+(u32)ty*4+(u32)(tx>>1); u8 b=M.vram[ta&0xFFFF]; u8 ci=(tx&1)?(b&15):(b>>4);
    out[x]= ci? (u8)(((e>>13)&3)*16+ci+((e&0x8000)?0x80:0)) : 0;
  }
}
static void render_window_line(int y,u8* outA){
  /* window plane replaces plane A in its region */
  int wh=M.reg[17], wv=M.reg[18]; int right=(wh>>7)&1, hp=(wh&0x1F)*16; int down=(wv>>7)&1, vp=(wv&0x1F)*8;
  int inv= down? (y>=vp) : (y<vp);
  u32 base=((u32)(M.reg[3]&0x3E)<<10); int ww=(M.reg[12]&1)?64:32;
  for(int x=0;x<320;x++){
    int inh= right? (x>=hp) : (x<hp);
    if(!(inv||inh)) continue;
    if(!inv && !inh) continue;
    u32 ea=base+(u32)(((y>>3)*ww+(x>>3))*2); u16 e=(u16)((M.vram[ea&0xFFFF]<<8)|M.vram[(ea+1)&0xFFFF]);
    int tx=x&7, ty=y&7; if(e&0x800) tx=7-tx; if(e&0x1000) ty=7-ty;
    u32 ta=((u32)(e&0x7FF)<<5)+(u32)ty*4+(u32)(tx>>1); u8 b=M.vram[ta&0xFFFF]; u8 ci=(tx&1)?(b&15):(b>>4);
    outA[x]= ci? (u8)(((e>>13)&3)*16+ci+((e&0x8000)?0x80:0)) : 0;
  }
}
static void render_sprites(int y,u8* out){
  memset(out,0,320);
  u32 sat=((u32)(M.reg[5]&0x7F))<<9; int link=0, n=0; int maxs=(M.reg[12]&1)?80:64;
  int drawn=0;
  do{
    u32 e=sat+(u32)link*8;
    int sy=((M.vram[e&0xFFFF]<<8)|M.vram[(e+1)&0xFFFF])&0x3FF; int sz=M.vram[(e+2)&0xFFFF]; link=M.vram[(e+3)&0xFFFF]&0x7F;
    u16 attr=(u16)((M.vram[(e+4)&0xFFFF]<<8)|M.vram[(e+5)&0xFFFF]); int sx=((M.vram[(e+6)&0xFFFF]<<8)|M.vram[(e+7)&0xFFFF])&0x1FF;
    int w=((sz>>2)&3)+1, h=(sz&3)+1; int top=sy-128;
    if(y>=top && y<top+h*8 && drawn<20){
      drawn++;
      int ly=y-top; if(attr&0x1000) ly=h*8-1-ly;
      for(int cx=0;cx<w*8;cx++){
        int X=sx-128+cx; if(X<0||X>=320) continue;
        int lx= (attr&0x800)? w*8-1-cx : cx;
        int tile=(attr&0x7FF)+(lx>>3)*h+(ly>>3);
        u32 ta=((u32)tile<<5)+(u32)(ly&7)*4+(u32)((lx&7)>>1); u8 b=M.vram[ta&0xFFFF]; u8 ci=(lx&1)?(b&15):(b>>4);
        if(ci && !out[X]) out[X]=(u8)(((attr>>13)&3)*16+ci+((attr&0x8000)?0x80:0));
      }
    }
    n++;
  }while(link && n<maxs);
}

#include "idle.h"
static void display(void){
 volatile uint16_t *pal=(void*)0x05000000;for(unsigned i=0;i<64;i++){unsigned c=M.cram[i];pal[i]=((c>>1)&7)*4|(((c>>5)&7)*4<<5)|(((c>>9)&7)*4<<10);}
 unsigned width=(M.reg[12]&1)?320:256,height=(M.reg[1]&8)?240:224;
 volatile uint16_t *o=(void*)0x06000000;
 for(unsigned dy=0;dy<160;dy++){int y=(dy*height)/160;render_plane(1,y,linebuf[0]);render_plane(0,y,linebuf[1]);render_window_line(y,linebuf[1]);render_sprites(y,linebuf[2]);
 for(unsigned dx=0;dx<240;dx+=2){unsigned packed=0;for(unsigned k=0;k<2;k++){unsigned x=((dx+k)*width)/240;u8 b=linebuf[0][x],a=linebuf[1][x],s=linebuf[2][x],c=M.reg[7]&63;
 if(s&128)c=s;else if(a&128)c=a;else if(b&128)c=b;else if(s)c=s;else if(a)c=a;else if(b)c=b;if(!(M.reg[1]&64))c=0;packed|=(c&63)<<(k*8);}*o++=packed;}}
 R16(0x04000000)=0x0404;
}
void md_main(void){
 const uint32_t*cfg=(void*)0x0203ff00;pager_init((void*)cfg[0]);romsize=cfg[1];R16(0x04000000)=0x80;R16(0x04000084)=0;
 R16(0x04000020)=R16(0x04000026)=0x100;R16(0x04000022)=R16(0x04000024)=0;R32(0x04000028)=R32(0x0400002c)=0;
 memset(&M,0,sizeof M);M.pad_th=0x40;m68k_reset();int hc=0;
 for(;;){unsigned keys=(~R16(0x04000130))&1023;M.pad=((keys>>6)&3)|((keys&32)>>3)|((keys&16)>>1)|((keys&2)<<3)|((keys&1)<<5)|((keys&256)>>2)|((keys&8)<<4);
 for(unsigned ln=0;ln<262;ln++){M.line=ln;if(ln==224)M.vint_pending=1;if(ln<=224){if(--hc<0){hc=M.reg[10];M.hint_pending=1;}}else hc=M.reg[10];u64 end=C.cycles+488;
 while(C.cycles<end){if(M.vint_pending&&(M.reg[1]&32)&&((C.sr>>8)&7)<6){M.vint_pending=0;m68k_interrupt(6);}else if(M.hint_pending&&(M.reg[0]&16)&&((C.sr>>8)&7)<4){M.hint_pending=0;m68k_interrupt(4);}if(C.stopped){C.cycles=end;break;}md_fold_ram_wait(end);if(C.cycles<end)m68k_step();}}
 M.frame++;display();volatile uint32_t*d=(void*)0x0203e000;d[0]=0x4d444558;d[1]=M.frame;d[2]=C.pc;d[3]=M.reg[1];}
}
