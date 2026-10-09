/* Minimal Mega Drive machine for recording PSIV state: 68K + VDP (no Z80/audio). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"

typedef struct {
  u8 ram[0x10000]; u8 vram[0x10000]; u16 cram[64]; u16 vsram[40]; u8 zram[0x2000]; u8 sram[0x10000];
  u8 reg[24]; int pending; u16 first; u32 addr; int code; int fill_pending;
  u16 status; int line; u64 frame; int vint_pending, hint_pending; int hcounter;
  u8 pad_th; u16 pad; /* active-high: U D L R B C A S */
  int sram_on; u8 ctrl1;
  M68K cpu;
  u32 hit_pc[16]; int nhit; u32 hit_count[16];
  int halted_on;
} MD;
static MD M;
static u8* rom; static u32 romsize;
static u32 fb[320*240];
static int trace_n; static u8 bo_cur[64][66]; static int bo_cur_n; static u8 bo_done[64][66]; static int bo_done_n; static int bo_frames; static u32 bs_log[4096][4]; static int bs_n; static int bs_on; static u32 snap_pc=0xFFFFFFFF; static u8 snap_buf[0x1000]; static int snap_count; static u32 snap_frame; static u32 break_pc=0xFFFFFFFF; static int break_hit; static u32 watch_lo, watch_hi; static int watch_on; static u32 watch_log[4096][3]; static int watch_n;

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
  if(a<0x400000){ if(M.sram_on && a>=0x200000 && a<0x210000) return M.sram[(a-0x200000)>>1]; return a<romsize?rom[a]:0xFF; }
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
  if(a<0x400000){ if(M.sram_on && a>=0x200000 && a<0x210000){ u8 b=M.sram[(a-0x200000)>>1]; return (u16)(0xFF00|b); } return a+1<romsize?(u16)((rom[a]<<8)|rom[a+1]):0xFFFF; }
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
static void check_watch(u32 a,u32 v,int sz){
  if(watch_on && a>=watch_lo && a<watch_hi && watch_n<4096){ watch_log[watch_n][0]=a|(u32)sz<<28; watch_log[watch_n][1]=v; watch_log[watch_n][2]=C.pc; watch_n++; }
}
void m68k_write8(u32 a,u8 v){
  a&=0xFFFFFF;
  if(a>=0xE00000){ check_watch(a,v,1); M.ram[a&0xFFFF]=v; return; }
  if(a>=0x200000&&a<0x210000){ if(M.sram_on) M.sram[(a-0x200000)>>1]=v; return; }
  if(a==0xA130F1){ M.sram_on=v&1; return; }
  if(a>=0xA00000&&a<0xA10000){ M.zram[a&0x1FFF]=v; return; }
  if(a>=0xA10000&&a<0xA10020){ u32 r=a&0x1F; if(r==0x03) M.pad_th=v; if(r==0x09) M.ctrl1=v; return; }
  if(a>=0xC00000&&a<0xC00020){ m68k_write16(a&~1u,(u16)(v|(v<<8))); return; }
}
void m68k_write16(u32 a,u16 v){
  a&=0xFFFFFE;
  if(a>=0xE00000){ check_watch(a,v,2); u32 o=a&0xFFFF; M.ram[o]=(u8)(v>>8); M.ram[o+1]=(u8)v; return; }
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
static u8 planeb_fb[224*320];   /* R46: plane B CRAM indices (0 = transparent) as displayed, per line */
u8* md_planeb(void){ return planeb_fb; }
static void render_line(int y){
  u32* o=fb+y*320;
  if(!(M.reg[1]&0x40)){ for(int x=0;x<320;x++) o[x]=0; memset(planeb_fb+y*320,0,320); return; }
  render_plane(1,y,linebuf[0]); render_plane(0,y,linebuf[1]); render_window_line(y,linebuf[1]); render_sprites(y,linebuf[2]);
  for(int x=0;x<320;x++) planeb_fb[y*320+x]=linebuf[0][x]&63;
  u16 bg=M.cram[M.reg[7]&63];
  for(int x=0;x<320;x++){
    u8 b=linebuf[0][x], a=linebuf[1][x], s=linebuf[2][x]; u8 c=0;
    /* priority order: S(hi) A(hi) B(hi) S A B */
    if(s&&(s&0x80)) c=s; else if(a&&(a&0x80)) c=a; else if(b&&(b&0x80)) c=b; else if(s) c=s; else if(a) c=a; else if(b) c=b;
    o[x]= c? cram_rgb(M.cram[c&63]) : cram_rgb(bg);
  }
}

/* ---------------- frame ---------------- */
static void run_cycles(u64 n){
  u64 end=C.cycles+n;
  while(C.cycles<end){
    if(M.vint_pending && (M.reg[1]&0x20) && ((C.sr>>8)&7)<6){ M.vint_pending=0; m68k_interrupt(6); }
    else if(M.hint_pending && (M.reg[0]&0x10) && ((C.sr>>8)&7)<4){ M.hint_pending=0; m68k_interrupt(4); }
    if(C.stopped){ C.cycles=end; break; }
    if(C.pc==break_pc){ break_hit=1; return; }
    if(C.pc==0x36676){ bo_cur_n=0; }
    if(C.pc==0x3679C && bo_cur_n<64){ u32 a=C.a[4]&0xFFFF; bo_cur[bo_cur_n][0]=(u8)((a-0xD000)/64); memcpy(&bo_cur[bo_cur_n][2],M.ram+a,64); bo_cur_n++; }
    if(C.pc==0x367B4){ memcpy(bo_done,bo_cur,sizeof bo_cur); bo_done_n=bo_cur_n; bo_frames++; }
    if(bs_on && (C.pc==0x3679C || C.pc==0x367A0) && bs_n<4096){ bs_log[bs_n][0]=C.pc; bs_log[bs_n][1]=C.a[4]; bs_log[bs_n][2]=C.a[3]; bs_log[bs_n][3]=(u32)M.frame; bs_n++; }
    if(C.pc==snap_pc){ memcpy(snap_buf,M.ram+0xD000,0x1000); snap_count++; snap_frame=(u32)M.frame; }
    for(int i=0;i<M.nhit;i++) if(C.pc==M.hit_pc[i]) M.hit_count[i]++;
    if(trace_n>0){ trace_n--; fprintf(stderr,"%06x sr=%04x d0=%08x d1=%08x a0=%08x a1=%08x a7=%08x\n",C.pc,C.sr,C.d[0],C.d[1],C.a[0],C.a[1],C.a[7]); }
    m68k_step();
  }
}
static int hint_counter;
void md_run_frames(int n){
  for(int f=0;f<n;f++){
    for(int ln=0;ln<262;ln++){
      M.line=ln;
      if(ln==224){ M.vint_pending=1; }
      if(ln<=224){ if(--hint_counter<0){ hint_counter=M.reg[10]; M.hint_pending=1; } } else hint_counter=M.reg[10];
      run_cycles(488);
      if(ln<224) render_line(ln);
      if(break_hit) return;
    }
    M.frame++;
  }
}
int md_load(const char* p){ FILE* f=fopen(p,"rb"); if(!f) return -1; fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET); free(rom); rom=malloc((size_t)n); if(fread(rom,1,(size_t)n,f)!=(size_t)n){fclose(f);return -2;} fclose(f); romsize=(u32)n; return 0; }
void md_reset(void){ memset(&M,0,sizeof M); m68k_reset(); M.pad_th=0x40; }
void md_set_pad(u32 m){ M.pad=(u16)m; }
u32* md_fb(void){ return fb; }
u8* md_ram(void){ return M.ram; } u8* md_vram(void){ return M.vram; } u16* md_cram(void){ return M.cram; } u16* md_vsram(void){ return M.vsram; } u8* md_regs(void){ return M.reg; } u8* md_sram(void){ return M.sram; }
u32 md_reg(int i){ if(i<8) return C.d[i]; if(i<16) return C.a[i-8]; if(i==16) return C.pc; if(i==17) return C.sr; return 0; }
void md_set_reg(int i,u32 v){ if(i<8) C.d[i]=v; else if(i<16) C.a[i-8]=v; else if(i==16) C.pc=v; else if(i==17) C.sr=(u16)v; }
u64 md_frame(void){ return M.frame; } u64 md_cycles(void){ return C.cycles; }
int md_save(const char* p){ FILE* f=fopen(p,"wb"); if(!f) return -1; M.cpu=C; fwrite(&M,sizeof M,1,f); fclose(f); return 0; }
int md_loadst(const char* p){ FILE* f=fopen(p,"rb"); if(!f) return -1; size_t k=fread(&M,sizeof M,1,f); fclose(f); C=M.cpu; return k==1?0:-2; }
void md_trace(int n){ trace_n=n; }
void md_patch_rom(u32 a,u8 v){ if(a<romsize) rom[a]=v; }
void md_watch(u32 lo,u32 hi){ watch_lo=lo|0xFF0000; watch_hi=hi|0xFF0000; watch_on=1; watch_n=0; }
int md_watch_n(void){ return watch_n; } u32* md_watch_log(void){ return &watch_log[0][0]; }
void md_set_hits(u32* pcs,int n){ M.nhit=n; for(int i=0;i<n;i++){ M.hit_pc[i]=pcs[i]; M.hit_count[i]=0; } }
u32 md_hit_count(int i){ return M.hit_count[i]; }
/* run single instructions until pc==target or limit; returns 1 if hit */
int md_run_to(u32 target,u64 maxcyc){ u64 end=C.cycles+maxcyc; while(C.cycles<end){ if(C.pc==target) return 1; m68k_step(); } return 0; }
void md_step(void){ m68k_step(); }
void md_set_break(u32 pc){ break_pc=pc; break_hit=0; }
int md_break_hit(void){ return break_hit; }
void md_set_snap(u32 pc){ snap_pc=pc; snap_count=0; }
int md_snap_count(void){ return snap_count; }
u8* md_snap_buf(void){ return snap_buf; }
void md_bs(int on){ bs_on=on; bs_n=0; }
int md_bs_n(void){ return bs_n; }
u32* md_bs_log(void){ return &bs_log[0][0]; }
int md_bo_n(void){ return bo_done_n; }
u8* md_bo_buf(void){ return &bo_done[0][0]; }
int md_bo_frames(void){ return bo_frames; }
