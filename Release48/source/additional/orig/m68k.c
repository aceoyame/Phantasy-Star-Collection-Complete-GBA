/* Compact MC68000 interpreter (user/supervisor, all base instructions). */
#include <stdint.h>

#include <string.h>
#include "m68k.h"

M68K C;
#define D C.d
#define A C.a
#define SR C.sr
#define FC_ 0x01
#define FV_ 0x02
#define FZ_ 0x04
#define FN_ 0x08
#define FX_ 0x10

static inline u32 msk(int sz){ return sz==1?0xFFu:sz==2?0xFFFFu:0xFFFFFFFFu; }
static inline u32 sgn(int sz){ return sz==1?0x80u:sz==2?0x8000u:0x80000000u; }
static inline s32 sext(u32 v,int sz){ return sz==1?(s32)(int8_t)v:sz==2?(s32)(int16_t)v:(s32)v; }

static inline u16 fetch16(void){ u16 v=m68k_read16(C.pc); C.pc+=2; return v; }
static inline u32 fetch32(void){ u32 v=m68k_read32(C.pc); C.pc+=4; return v; }

static inline u32 rd(u32 a,int sz){ return sz==1?m68k_read8(a):sz==2?m68k_read16(a):m68k_read32(a); }
static inline void wr(u32 a,int sz,u32 v){ if(sz==1) m68k_write8(a,(u8)v); else if(sz==2) m68k_write16(a,(u16)v); else m68k_write32(a,v); }

void m68k_set_sr(u16 v){
  int olds=(SR>>13)&1, news=(v>>13)&1;
  if(olds!=news){ if(news){ C.usp=A[7]; A[7]=C.ssp; } else { C.ssp=A[7]; A[7]=C.usp; } }
  SR=v&0xA71F;
}
static void push32(u32 v){ A[7]-=4; m68k_write32(A[7],v); }
static void push16(u16 v){ A[7]-=2; m68k_write16(A[7],v); }
static u32 pop32(void){ u32 v=m68k_read32(A[7]); A[7]+=4; return v; }
static u16 pop16(void){ u16 v=m68k_read16(A[7]); A[7]+=2; return v; }

void m68k_exception(int vec){
  u16 old=SR; m68k_set_sr((u16)((SR|0x2000)&~0x8000));
  push32(C.pc); push16(old);
  C.pc=m68k_read32((u32)vec*4u); C.stopped=0;
}
void m68k_interrupt(int level){
  if(level<=((SR>>8)&7) && level!=7) return;
  u16 old=SR; m68k_set_sr((u16)(((SR|0x2000)&~0x8000&~0x0700)|(level<<8)));
  push32(C.pc); push16(old);
  C.pc=m68k_read32((u32)(24+level)*4u); C.stopped=0; C.cycles+=44;
}
void m68k_reset(void){
  memset(&C,0,sizeof C); SR=0x2700; C.ssp=A[7]=m68k_read32(0); C.pc=m68k_read32(4);
}

/* ---- effective address ---- */
typedef struct { int kind; u32 addr; int reg; } EA; /* kind 0=Dn 1=An 2=mem 3=imm */
static u32 ext_index(u32 base){
  u16 e=fetch16(); int r=(e>>12)&7; s32 x;
  if(e&0x8000) x= (e&0x800)? (s32)A[r] : (s32)(int16_t)A[r];
  else x= (e&0x800)? (s32)D[r] : (s32)(int16_t)D[r];
  return base+(u32)x+(u32)(s32)(int8_t)(e&0xFF);
}
static EA ea_get(int mode,int reg,int sz){
  EA e={0,0,reg};
  switch(mode){
    case 0: e.kind=0; return e;
    case 1: e.kind=1; return e;
    case 2: e.kind=2; e.addr=A[reg]; return e;
    case 3: e.kind=2; e.addr=A[reg]; A[reg]+= (reg==7&&sz==1)?2:(u32)sz; return e;
    case 4: e.kind=2; A[reg]-= (reg==7&&sz==1)?2:(u32)sz; e.addr=A[reg]; C.cycles+=2; return e;
    case 5: e.kind=2; e.addr=A[reg]+(u32)(s32)(int16_t)fetch16(); C.cycles+=4; return e;
    case 6: e.kind=2; e.addr=ext_index(A[reg]); C.cycles+=6; return e;
    case 7:
      switch(reg){
        case 0: e.kind=2; e.addr=(u32)(s32)(int16_t)fetch16(); C.cycles+=4; return e;
        case 1: e.kind=2; e.addr=fetch32(); C.cycles+=8; return e;
        case 2: { u32 b=C.pc; e.kind=2; e.addr=b+(u32)(s32)(int16_t)fetch16(); C.cycles+=4; return e; }
        case 3: { u32 b=C.pc; e.kind=2; e.addr=ext_index(b); C.cycles+=6; return e; }
        case 4: e.kind=3; if(sz==4) e.addr=fetch32(); else { u16 w=fetch16(); e.addr= sz==1? (w&0xFF) : w; } C.cycles+=4; return e;
      }
  }
   e.kind=3; return e;
}
static u32 ea_rd(EA* e,int sz){
  switch(e->kind){ case 0: return D[e->reg]&msk(sz); case 1: return A[e->reg]&msk(sz); case 2: C.cycles+= sz==4?8:4; return rd(e->addr,sz); default: return e->addr&msk(sz); }
}
static void ea_wr(EA* e,int sz,u32 v){
  switch(e->kind){
    case 0: { u32 m=msk(sz); D[e->reg]=(D[e->reg]&~m)|(v&m); return; }
    case 1: A[e->reg]=v; return;
    case 2: C.cycles+= sz==4?8:4; wr(e->addr,sz,v); return;
  }
}
static int sz_from2(int s){ return s==0?1:s==1?2:s==2?4:0; }

/* ---- flags ---- */
static inline void nz(u32 v,int sz){ v&=msk(sz); SR&=~(FN_|FZ_|FV_|FC_); if(!v) SR|=FZ_; if(v&sgn(sz)) SR|=FN_; }
static u32 do_add_(u32 s,u32 d,int sz,int x,int setx,int isx){
  u32 m=msk(sz); u64 r=(u64)(s&m)+(d&m)+(u64)x; u32 res=(u32)r&m; u32 sb=sgn(sz);
  int c=(r>>(sz*8))&1; int v=((~(s^d))&(s^res)&sb)!=0;
  SR&=~(FN_|FV_|FC_|(setx?FX_:0)|(isx?0:FZ_)); if(c){ SR|=FC_; if(setx) SR|=FX_; } if(v) SR|=FV_; if(res&sb) SR|=FN_;
  if(!isx){ if(!res) SR|=FZ_; } else { if(res) SR&=~FZ_; }
  return res;
}
static u32 do_sub_(u32 s,u32 d,int sz,int x,int setx,int isx){ /* d - s - x */
  u32 m=msk(sz); u64 r=(u64)(d&m)-(u64)(s&m)-(u64)x; u32 res=(u32)r&m; u32 sb=sgn(sz);
  int c=(r>>63)&1; int v=((s^d)&(d^res)&sb)!=0;
  SR&=~(FN_|FV_|FC_|(setx?FX_:0)|(isx?0:FZ_)); if(c){ SR|=FC_; if(setx) SR|=FX_; } if(v) SR|=FV_; if(res&sb) SR|=FN_;
  if(!isx){ if(!res) SR|=FZ_; } else { if(res) SR&=~FZ_; }
  return res;
}
#define do_add(s,d,sz,x,setx) do_add_(s,d,sz,x,setx,0)
#define do_sub(s,d,sz,x,setx) do_sub_(s,d,sz,x,setx,0)
static void do_cmp(u32 s,u32 d,int sz){ u16 x=SR&FX_; do_sub(s,d,sz,0,0); SR=(u16)((SR&~FX_)|x); }

static int cond(int c){
  int Cf=SR&FC_?1:0,V=SR&FV_?1:0,Z=SR&FZ_?1:0,N=SR&FN_?1:0;
  switch(c){case 0:return 1;case 1:return 0;case 2:return !Cf&&!Z;case 3:return Cf||Z;case 4:return !Cf;case 5:return Cf;case 6:return !Z;case 7:return Z;
  case 8:return !V;case 9:return V;case 10:return !N;case 11:return N;case 12:return N==V;case 13:return N!=V;case 14:return !Z&&N==V;case 15:return Z||N!=V;}
  return 0;
}

static void illegal(u16 op){  C.pc-=2; m68k_exception(4); }

static u8 bcd_add(u8 s,u8 d){
  int x=(SR&FX_)?1:0; int lo=(s&15)+(d&15)+x; int r=(s&0xF0)+(d&0xF0)+lo; if(lo>9) r+=6; int c=0; if(r>0x99){ r+=0x60; c=1; }
  SR&=~(FC_|FX_); if(c) SR|=FC_|FX_; if(r&0xFF) SR&=~FZ_; return (u8)r;
}
static u8 bcd_sub(u8 s,u8 d){ /* d - s - x */
  int x=(SR&FX_)?1:0; int lo=(d&15)-(s&15)-x; int r=(d&0xF0)-(s&0xF0)+lo; if(lo<0) r-=6; int c=0; if(r<0){ r-=0x60; c=1; }
  SR&=~(FC_|FX_); if(c) SR|=FC_|FX_; if(r&0xFF) SR&=~FZ_; return (u8)r;
}

/* ---- shifts ---- */
static u32 shift(int type,int left,u32 v,int cnt,int sz){
  u32 m=msk(sz), sb=sgn(sz); v&=m; int bits=sz*8;
  SR&=~(FV_|FC_);
  if(cnt==0){ if(type==2){ if(SR&FX_) SR|=FC_; } nz(v,sz); SR=(u16)(SR&~FC_)|((type==2&&(SR&FX_))?FC_:0); return v; }
  int c=0,vf=0; u32 x=(SR&FX_)?1:0;
  for(int i=0;i<cnt;i++){
    switch(type){
      case 0: /* AS */ if(left){ c=(v&sb)?1:0; u32 nv=(v<<1)&m; if((nv&sb)!=(v&sb)) vf=1; v=nv; } else { c=v&1; v=(v>>1)|(v&sb); } x=(u32)c; break;
      case 1: /* LS */ if(left){ c=(v&sb)?1:0; v=(v<<1)&m; } else { c=v&1; v>>=1; } x=(u32)c; break;
      case 2: /* ROX */ if(left){ c=(v&sb)?1:0; v=((v<<1)|x)&m; } else { c=v&1; v=(v>>1)|(x?sb:0); } x=(u32)c; break;
      case 3: /* RO */ if(left){ c=(v&sb)?1:0; v=((v<<1)|(u32)c)&m; } else { c=v&1; v=(v>>1)|(c?sb:0); } break;
    }
  }
  (void)bits;
  u16 keepx=SR&FX_; nz(v,sz); if(c) SR|=FC_; if(vf) SR|=FV_;
  if(type!=3){ SR&=~FX_; if(x) SR|=FX_; } else SR|=keepx;
  C.cycles+=2*(u64)cnt;
  return v;
}

/* ---- MOVEM ---- */
static void movem(u16 op){
  int dir=(op>>10)&1, sz=(op&0x40)?4:2; int mode=(op>>3)&7, reg=op&7; u16 mask=fetch16();
  if(mode==4){ /* predecrement, store only */
    u32 a=A[reg];
    for(int i=15;i>=0;i--) if(mask&(1u<<(15-i))){ a-=(u32)sz; u32 v= i<8?D[i]:A[i-8]; wr(a,sz,v); C.cycles+=sz==4?8:4; }
    A[reg]=a; return;
  }
  u32 a;
  if(mode==3) a=A[reg]; else { EA e=ea_get(mode,reg,sz); a=e.addr; }
  for(int i=0;i<16;i++) if(mask&(1u<<i)){
    if(dir){ u32 v=rd(a,sz); if(sz==2) v=(u32)(s32)(int16_t)v; if(i<8) D[i]=v; else A[i-8]=v; }
    else { u32 v= i<8?D[i]:A[i-8]; wr(a,sz,v); }
    a+=(u32)sz; C.cycles+=sz==4?8:4;
  }
  if(mode==3) A[reg]=a;
}

static void line0(u16 op){
  int mode=(op>>3)&7, reg=op&7;
  if(op&0x100){ /* dynamic bit ops or movep */
    if(mode==1){ /* MOVEP */
      int dr=(op>>9)&7; u32 a=A[reg]+(u32)(s32)(int16_t)fetch16(); int opm=(op>>6)&3;
      switch(opm){
        case 0: D[dr]=(D[dr]&0xFFFF0000)|(u32)(m68k_read8(a)<<8)|m68k_read8(a+2); break;
        case 1: D[dr]=((u32)m68k_read8(a)<<24)|((u32)m68k_read8(a+2)<<16)|((u32)m68k_read8(a+4)<<8)|m68k_read8(a+6); break;
        case 2: m68k_write8(a,(u8)(D[dr]>>8)); m68k_write8(a+2,(u8)D[dr]); break;
        case 3: m68k_write8(a,(u8)(D[dr]>>24)); m68k_write8(a+2,(u8)(D[dr]>>16)); m68k_write8(a+4,(u8)(D[dr]>>8)); m68k_write8(a+6,(u8)D[dr]); break;
      }
      return;
    }
    int bitn=(int)D[(op>>9)&7]; int t=(op>>6)&3;
    if(mode==0){ bitn&=31; u32 v=D[reg]; if(v&(1u<<bitn)) SR&=~FZ_; else SR|=FZ_;
      if(t==1) D[reg]^=1u<<bitn; else if(t==2) D[reg]&=~(1u<<bitn); else if(t==3) D[reg]|=1u<<bitn; return; }
    bitn&=7; EA e=ea_get(mode,reg,1); u32 v=ea_rd(&e,1); if(v&(1u<<bitn)) SR&=~FZ_; else SR|=FZ_;
    if(t==1) ea_wr(&e,1,v^(1u<<bitn)); else if(t==2) ea_wr(&e,1,v&~(1u<<bitn)); else if(t==3) ea_wr(&e,1,v|(1u<<bitn));
    return;
  }
  int which=(op>>9)&7;
  if(which==4){ /* static bit */
    int bitn=fetch16()&0xFF; int t=(op>>6)&3;
    if(mode==0){ bitn&=31; u32 v=D[reg]; if(v&(1u<<bitn)) SR&=~FZ_; else SR|=FZ_;
      if(t==1) D[reg]^=1u<<bitn; else if(t==2) D[reg]&=~(1u<<bitn); else if(t==3) D[reg]|=1u<<bitn; return; }
    bitn&=7; EA e=ea_get(mode,reg,1); u32 v=ea_rd(&e,1); if(v&(1u<<bitn)) SR&=~FZ_; else SR|=FZ_;
    if(t==1) ea_wr(&e,1,v^(1u<<bitn)); else if(t==2) ea_wr(&e,1,v&~(1u<<bitn)); else if(t==3) ea_wr(&e,1,v|(1u<<bitn));
    return;
  }
  int sz=sz_from2((op>>6)&3); if(!sz){ illegal(op); return; }
  /* to CCR / SR */
  if(mode==7&&reg==4){
    u16 imm=fetch16();
    if(sz==1){ if(which==0) SR|=imm&0x1F; else if(which==1) SR&=(u16)(0xFF00|imm); else if(which==5) SR^=imm&0x1F; else illegal(op); return; }
    if(sz==2){ if(!(SR&0x2000)){ m68k_exception(8); return; } u16 v=SR; if(which==0) v|=imm; else if(which==1) v&=imm; else if(which==5) v^=imm; else { illegal(op); return; } m68k_set_sr(v); return; }
  }
  u32 imm= sz==4? fetch32() : (fetch16()&msk(sz));
  EA e=ea_get(mode,reg,sz); u32 d=ea_rd(&e,sz), r;
  switch(which){
    case 0: r=d|imm; nz(r,sz); ea_wr(&e,sz,r); break;
    case 1: r=d&imm; nz(r,sz); ea_wr(&e,sz,r); break;
    case 2: r=do_sub(imm,d,sz,0,1); ea_wr(&e,sz,r); break;
    case 3: r=do_add(imm,d,sz,0,1); ea_wr(&e,sz,r); break;
    case 5: r=d^imm; nz(r,sz); ea_wr(&e,sz,r); break;
    case 6: do_cmp(imm,d,sz); break;
    default: illegal(op);
  }
}

static void line4(u16 op){
  int mode=(op>>3)&7, reg=op&7;
  if((op&0xF1C0)==0x41C0){ EA e=ea_get(mode,reg,4); A[(op>>9)&7]=e.addr; return; } /* LEA */
  if((op&0xF1C0)==0x4180){ /* CHK.W */ EA e=ea_get(mode,reg,2); s32 b=(int16_t)ea_rd(&e,2); s32 v=(int16_t)D[(op>>9)&7];
    if(v<0){ SR|=FN_; m68k_exception(6); } else if(v>b){ SR&=~FN_; m68k_exception(6); } return; }
  switch(op&0xFFC0){
    case 0x40C0: { EA e=ea_get(mode,reg,2); ea_wr(&e,2,SR); return; } /* move from SR */
    case 0x44C0: { EA e=ea_get(mode,reg,2); u16 v=(u16)ea_rd(&e,2); SR=(u16)((SR&0xFF00)|(v&0x1F)); return; } /* to CCR */
    case 0x46C0: { if(!(SR&0x2000)){ m68k_exception(8); return; } EA e=ea_get(mode,reg,2); m68k_set_sr((u16)ea_rd(&e,2)); return; }
    case 0x4800: { EA e=ea_get(mode,reg,1); u8 v=(u8)ea_rd(&e,1); u8 r=bcd_sub(v,0); SR=(u16)((SR&~FN_)|((r&0x80)?FN_:0)); ea_wr(&e,1,r); return; } /* NBCD */
    case 0x4840: if(mode==0){ u32 v=D[reg]; v=(v>>16)|(v<<16); D[reg]=v; nz(v,4); return; } /* SWAP */
                 { EA e=ea_get(mode,reg,4); push32(e.addr); return; } /* PEA */
    case 0x4880: if(mode==0){ u32 v=(u32)(s32)(int8_t)D[reg]; D[reg]=(D[reg]&0xFFFF0000)|(v&0xFFFF); nz(v,2); return; } movem(op); return;
    case 0x48C0: if(mode==0){ u32 v=(u32)(s32)(int16_t)D[reg]; D[reg]=v; nz(v,4); return; } movem(op); return;
    case 0x4CC0: case 0x4C80: movem(op); return;
    case 0x4AC0: { if(op==0x4AFC){ illegal(op); return; } EA e=ea_get(mode,reg,1); u32 v=ea_rd(&e,1); nz(v,1); ea_wr(&e,1,v|0x80); return; } /* TAS */
    case 0x4E80: { EA e=ea_get(mode,reg,4); push32(C.pc); C.pc=e.addr; C.cycles+=8; return; } /* JSR */
    case 0x4EC0: { EA e=ea_get(mode,reg,4); C.pc=e.addr; return; } /* JMP */
  }
  if((op&0xFF00)==0x4000||(op&0xFF00)==0x4200||(op&0xFF00)==0x4400||(op&0xFF00)==0x4600||(op&0xFF00)==0x4A00){
    int sz=sz_from2((op>>6)&3); EA e=ea_get(mode,reg,sz); u32 v=ea_rd(&e,sz), r;
    switch(op&0xFF00){
      case 0x4000: r=do_sub_(v,0,sz,(SR&FX_)?1:0,1,1); ea_wr(&e,sz,r); return; /* NEGX */
      case 0x4200: ea_wr(&e,sz,0); SR=(u16)((SR&~(FN_|FV_|FC_))|FZ_); return; /* CLR */
      case 0x4400: r=do_sub(v,0,sz,0,1); ea_wr(&e,sz,r); return; /* NEG */
      case 0x4600: r=~v; nz(r,sz); ea_wr(&e,sz,r); return; /* NOT */
      case 0x4A00: nz(v,sz); return; /* TST */
    }
  }
  if((op&0xFFF0)==0x4E40){ m68k_exception(32+(op&15)); return; } /* TRAP */
  if((op&0xFFF8)==0x4E50){ push32(A[reg]); A[reg]=A[7]; A[7]+=(u32)(s32)(int16_t)fetch16(); return; } /* LINK */
  if((op&0xFFF8)==0x4E58){ A[7]=A[reg]; A[reg]=pop32(); return; } /* UNLK */
  if((op&0xFFF8)==0x4E60){ C.usp=A[reg]; return; }
  if((op&0xFFF8)==0x4E68){ A[reg]=C.usp; return; }
  switch(op){
    case 0x4E70: return; /* RESET */
    case 0x4E71: return; /* NOP */
    case 0x4E72: { u16 v=fetch16(); m68k_set_sr(v); C.stopped=1; return; }
    case 0x4E73: { u16 s=pop16(); u32 pc=pop32(); m68k_set_sr(s); C.pc=pc; return; }
    case 0x4E75: C.pc=pop32(); C.cycles+=8; return;
    case 0x4E76: if(SR&FV_) m68k_exception(7); return;
    case 0x4E77: { u16 s=pop16(); SR=(u16)((SR&0xFF00)|(s&0x1F)); C.pc=pop32(); return; }
  }
  illegal(op);
}

static void line5(u16 op){
  int mode=(op>>3)&7, reg=op&7;
  if(((op>>6)&3)==3){
    int c=(op>>8)&15;
    if(mode==1){ /* DBcc */
      s16 disp=(s16)fetch16();
      if(!cond(c)){ u16 w=(u16)(D[reg]-1); D[reg]=(D[reg]&0xFFFF0000)|w; if(w!=0xFFFF) C.pc=C.pc-2+(u32)(s32)disp; }
      return;
    }
    EA e=ea_get(mode,reg,1); ea_wr(&e,1,cond(c)?0xFF:0); return;
  }
  int sz=sz_from2((op>>6)&3); u32 q=(op>>9)&7; if(!q) q=8;
  if(mode==1){ if(op&0x100) A[reg]-=q; else A[reg]+=q; return; }
  EA e=ea_get(mode,reg,sz); u32 v=ea_rd(&e,sz);
  u32 r= (op&0x100)? do_sub(q,v,sz,0,1) : do_add(q,v,sz,0,1); ea_wr(&e,sz,r);
}

static void line6(u16 op){
  int c=(op>>8)&15; s32 disp=(int8_t)(op&0xFF); u32 base=C.pc;
  if(!disp) disp=(int16_t)fetch16();
  if(c==1){ push32(C.pc); C.pc=base+(u32)disp; C.cycles+=8; return; }
  if(cond(c)){ C.pc=base+(u32)disp; C.cycles+=2; }
}

static void line8(u16 op){ /* OR / DIVU / DIVS / SBCD */
  int dr=(op>>9)&7, mode=(op>>3)&7, reg=op&7, opm=(op>>6)&7;
  if(opm==3||opm==7){
    EA e=ea_get(mode,reg,2); u32 s=ea_rd(&e,2)&0xFFFF; C.cycles+=120;
    if(!s){ m68k_exception(5); return; }
    if(opm==3){ u32 q=D[dr]/s, r=D[dr]%s; if(q>0xFFFF){ SR|=FV_; SR&=~FC_; return; } D[dr]=(r<<16)|q; nz(q,2); }
    else { s32 dv=(s32)D[dr], sv=(int16_t)s; if(dv==(s32)0x80000000 && sv==-1){ SR|=FV_; return; } s32 q=dv/sv, r=dv%sv; if(q>32767||q<-32768){ SR|=FV_; SR&=~FC_; return; } D[dr]=((u32)(r&0xFFFF)<<16)|((u32)q&0xFFFF); nz((u32)q,2); }
    return;
  }
  if((op&0x1F0)==0x100){ /* SBCD */
    if(op&8){ A[reg]-= reg==7?2:1; A[dr]-= dr==7?2:1; u8 s=m68k_read8(A[reg]), d=m68k_read8(A[dr]); m68k_write8(A[dr],bcd_sub(s,d)); }
    else { u8 r=bcd_sub((u8)D[reg],(u8)D[dr]); D[dr]=(D[dr]&~0xFFu)|r; }
    return;
  }
  int sz=sz_from2(opm&3);
  if(opm<3){ EA e=ea_get(mode,reg,sz); u32 r=(D[dr]|ea_rd(&e,sz))&msk(sz); nz(r,sz); D[dr]=(D[dr]&~msk(sz))|r; }
  else { EA e=ea_get(mode,reg,sz); u32 r=(D[dr]|ea_rd(&e,sz))&msk(sz); nz(r,sz); ea_wr(&e,sz,r); }
}

static void addsub(u16 op,int sub){
  int dr=(op>>9)&7, mode=(op>>3)&7, reg=op&7, opm=(op>>6)&7;
  if(opm==3||opm==7){ int sz=opm==3?2:4; EA e=ea_get(mode,reg,sz); u32 s=ea_rd(&e,sz); if(sz==2) s=(u32)(s32)(int16_t)s; if(sub) A[dr]-=s; else A[dr]+=s; return; }
  int sz=sz_from2(opm&3);
  if(opm>=4 && (mode==0||mode==1)){ /* ADDX/SUBX */
    int x=(SR&FX_)?1:0;
    if(mode==0){ u32 r= sub? do_sub_(D[reg],D[dr],sz,x,1,1) : do_add_(D[reg],D[dr],sz,x,1,1); D[dr]=(D[dr]&~msk(sz))|r; }
    else { u32 st=(sz==1&&reg==7)?2:(u32)sz, dt=(sz==1&&dr==7)?2:(u32)sz; A[reg]-=st; u32 s=rd(A[reg],sz); A[dr]-=dt; u32 d=rd(A[dr],sz); u32 r= sub? do_sub_(s,d,sz,x,1,1):do_add_(s,d,sz,x,1,1); wr(A[dr],sz,r); }
    return;
  }
  if(opm<3){ EA e=ea_get(mode,reg,sz); u32 s=ea_rd(&e,sz); u32 r= sub? do_sub(s,D[dr],sz,0,1) : do_add(s,D[dr],sz,0,1); D[dr]=(D[dr]&~msk(sz))|r; }
  else { EA e=ea_get(mode,reg,sz); u32 d=ea_rd(&e,sz); u32 r= sub? do_sub(D[dr],d,sz,0,1) : do_add(D[dr],d,sz,0,1); ea_wr(&e,sz,r); }
}

static void lineB(u16 op){
  int dr=(op>>9)&7, mode=(op>>3)&7, reg=op&7, opm=(op>>6)&7;
  if(opm==3||opm==7){ int sz=opm==3?2:4; EA e=ea_get(mode,reg,sz); u32 s=ea_rd(&e,sz); if(sz==2) s=(u32)(s32)(int16_t)s; do_cmp(s,A[dr],4); return; }
  int sz=sz_from2(opm&3);
  if(opm<3){ EA e=ea_get(mode,reg,sz); u32 s=ea_rd(&e,sz); do_cmp(s,D[dr],sz); return; }
  if(mode==1){ /* CMPM */ u32 st=(sz==1&&reg==7)?2:(u32)sz, dt=(sz==1&&dr==7)?2:(u32)sz; u32 s=rd(A[reg],sz); A[reg]+=st; u32 d=rd(A[dr],sz); A[dr]+=dt; do_cmp(s,d,sz); return; }
  EA e=ea_get(mode,reg,sz); u32 r=(ea_rd(&e,sz)^D[dr])&msk(sz); nz(r,sz); ea_wr(&e,sz,r); /* EOR */
}

static void lineC(u16 op){
  int dr=(op>>9)&7, mode=(op>>3)&7, reg=op&7, opm=(op>>6)&7;
  if(opm==3||opm==7){ EA e=ea_get(mode,reg,2); u32 s=ea_rd(&e,2); u32 r; if(opm==3) r=(D[dr]&0xFFFF)*(s&0xFFFF); else r=(u32)((s32)(int16_t)D[dr]*(s32)(int16_t)s); D[dr]=r; nz(r,4); C.cycles+=50; return; }
  if((op&0x1F0)==0x100){ /* ABCD */
    if(op&8){ A[reg]-= reg==7?2:1; A[dr]-= dr==7?2:1; u8 s=m68k_read8(A[reg]), d=m68k_read8(A[dr]); m68k_write8(A[dr],bcd_add(s,d)); }
    else { u8 r=bcd_add((u8)D[reg],(u8)D[dr]); D[dr]=(D[dr]&~0xFFu)|r; }
    return;
  }
  if((op&0x1F8)==0x140){ u32 t=D[dr]; D[dr]=D[reg]; D[reg]=t; return; }
  if((op&0x1F8)==0x148){ u32 t=A[dr]; A[dr]=A[reg]; A[reg]=t; return; }
  if((op&0x1F8)==0x188){ u32 t=D[dr]; D[dr]=A[reg]; A[reg]=t; return; }
  int sz=sz_from2(opm&3);
  if(opm<3){ EA e=ea_get(mode,reg,sz); u32 r=(D[dr]&ea_rd(&e,sz))&msk(sz); nz(r,sz); D[dr]=(D[dr]&~msk(sz))|r; }
  else { EA e=ea_get(mode,reg,sz); u32 r=(D[dr]&ea_rd(&e,sz))&msk(sz); nz(r,sz); ea_wr(&e,sz,r); }
}

static void lineE(u16 op){
  if(((op>>6)&3)==3){ /* memory shift, word, by 1 */
    int type=(op>>9)&3, left=(op>>8)&1; EA e=ea_get((op>>3)&7,op&7,2); u32 v=ea_rd(&e,2); ea_wr(&e,2,shift(type,left,v,1,2)); return;
  }
  int sz=sz_from2((op>>6)&3), left=(op>>8)&1, type=(op>>3)&3, reg=op&7; int cnt=(op>>9)&7;
  if(op&0x20) cnt=(int)(D[cnt]&63); else if(!cnt) cnt=8;
  u32 r=shift(type,left,D[reg],cnt,sz); D[reg]=(D[reg]&~msk(sz))|(r&msk(sz));
}

void m68k_step(void){
  u16 op=fetch16(); C.cycles+=4;
  switch(op>>12){
    case 0: line0(op); break;
    case 1: case 2: case 3: {
      int sz= (op>>12)==1?1:(op>>12)==3?2:4;
      EA s=ea_get((op>>3)&7,op&7,sz); u32 v=ea_rd(&s,sz);
      int dm=(op>>6)&7, dreg=(op>>9)&7;
      if(dm==1){ A[dreg]= sz==2? (u32)(s32)(int16_t)v : v; break; } /* MOVEA */
      EA d=ea_get(dm,dreg,sz); ea_wr(&d,sz,v); nz(v,sz); break; }
    case 4: line4(op); break;
    case 5: line5(op); break;
    case 6: line6(op); break;
    case 7: if(op&0x100){ illegal(op); break; } { u32 v=(u32)(s32)(int8_t)(op&0xFF); D[(op>>9)&7]=v; nz(v,4); } break;
    case 8: line8(op); break;
    case 9: addsub(op,1); break;
    case 10: C.pc-=2; m68k_exception(10); break;
    case 11: lineB(op); break;
    case 12: lineC(op); break;
    case 13: addsub(op,0); break;
    case 14: lineE(op); break;
    case 15: C.pc-=2; m68k_exception(11); break;
  }
}
