/* Runtime support for statically recompiled 68000 code (aot68k.py output).
 * Semantics and cycle accounting mirror m68k_ref.c exactly; memory fast paths mirror m68k.c. */
#pragma once
#include "../md.h"

extern u32 aot_lim;                      /* cycle limit of the current m68k_exec call */
extern u32 aot_rgen[128];                 /* write generation of each 256-byte block of 0xFF0000-0xFF7FFF (RAM code window) */
u32 md_native_gather_rows(u32 room,u32 sr);
u32 md_native_shifted_nibbles(u32 room,u32 sr);
#define AD C.d
#define AA C.a
#define AINL static inline __attribute__((always_inline))
#ifdef MD_HOST
#define AOT_HOT
#else
#define AOT_HOT __attribute__((section(".hot")))
#endif

/* memory: out-of-line (m68k.c fast paths, IWRAM); constant RAM addresses are inlined by the generator */
u32 RD8(u32 a); u32 RD16(u32 a); u32 RD32(u32 a); void WR8(u32 a,u32 v); void WR16(u32 a,u32 v); void WR32(u32 a,u32 v);
#define R8 RD8
#define R16 RD16
AINL u32 aot_read32(u32 a) {
  a&=0xfffffeu;
  if(a>=0xe00000u && (a&0xffffu)<0xfffcu) {
    const u16 *p=m68k_ram+((a&0xffffu)>>1);
    return ((u32)p[0]<<16)|p[1];
  }
  return RD32(a);
}
#define R32 aot_read32
#define W8 WR8
#define W16 WR16
#define W32 WR32
#define RAM8(o)  (((u8*)md_ram)[(o)^1u])
#define RAM16(o) (md_ram[(o)>>1])

/* Native implementation of the shared RAM -> VRAM transfer body.  Only
 * complete MOVE instructions before the original cycle boundary are batched;
 * the generated DBRA and remaining code retain their existing timing. */
#define NATIVE_VRAM_COPY(mx,ix) do { \
  u32 _room=lim-cyc,_n=_room>=20u*(mx)?(mx):(_room+19u)/20u; \
  if((AA[5]&0xFFFFFEu)==0xC00000u&&md_native_vram_copy(AA[0],_n)){ \
    AA[0]+=_n*4u;cyc+=_n*20u; \
    if(_n==(mx))goto B3228; \
    i=(ix)+_n;goto ex; \
  } \
}while(0)

/* flags (on the local sr) */
#define FC_ 1u
#define FV_ 2u
#define FZ_ 4u
#define FN_ 8u
#define FX_ 16u
#define NZ(v,m,sb) (sr=(sr&~(FN_|FZ_|FV_|FC_))|(((v)&(m))?0u:FZ_)|(((v)&(sb))?FN_:0u))
/* NZ restricted to the live flags L (flags not in L are dead: never read before being written again) */
#define NZL(L,v,m,sb) (sr=(sr&~((L)&15u))|((((L)&FZ_)&&!((v)&(m)))?FZ_:0u)|((((L)&FN_)&&((v)&(sb)))?FN_:0u))
AINL u32 f_add(u32*psr,u32 s,u32 d,u32 m,u32 sb,u32 bits,u32 x,int setx,int isx,u32 L){
  u32 res,c;
  if(bits==32u){ res=s+d+x; c=x?(res<=s):(res<s); } else { u32 t=(s&m)+(d&m)+x; res=t&m; c=(t>>bits)&1u; }
  u32 v=(((~(s^d))&(s^res)&sb)!=0u);
  u32 fl=c|(v<<1)|((res&sb)?FN_:0u)|(isx?0u:(res?0u:FZ_))|(setx?(c<<4):0u);
  u32 clr=(FN_|FV_|FC_|(setx?FX_:0u)|(isx?0u:FZ_))&L;
  u32 sr=(*psr&~clr)|(fl&clr); if(isx&&res&&(L&FZ_)) sr&=~FZ_;
  *psr=sr; return res; }
AINL u32 f_sub(u32*psr,u32 s,u32 d,u32 m,u32 sb,u32 bits,u32 x,int setx,int isx,u32 L){
  u32 res,c;
  if(bits==32u){ res=d-s-x; c=x?(d<=s):(d<s); } else { u32 t=(d&m)-(s&m)-x; res=t&m; c=(t>>bits)&1u; }
  u32 v=(((s^d)&(d^res)&sb)!=0u);
  u32 fl=c|(v<<1)|((res&sb)?FN_:0u)|(isx?0u:(res?0u:FZ_))|(setx?(c<<4):0u);
  u32 clr=(FN_|FV_|FC_|(setx?FX_:0u)|(isx?0u:FZ_))&L;
  u32 sr=(*psr&~clr)|(fl&clr); if(isx&&res&&(L&FZ_)) sr&=~FZ_;
  *psr=sr; return res; }
#define ADD(s,d,m,sb,bits,x,setx,isx) f_add(&sr,(s),(d),(m),(sb),(bits),(x),(setx),(isx),31u)
#define ADDL(L,s,d,m,sb,bits,x,setx,isx) f_add(&sr,(s),(d),(m),(sb),(bits),(x),(setx),(isx),(L))
#define SUB(s,d,m,sb,x,setx,isx) f_sub(&sr,(s),(d),(m),(sb),((m)==0xFFFFFFFFu?32u:(m)==0xFFFFu?16u:8u),(x),(setx),(isx),31u)
#define SUBL(L,s,d,m,sb,x,setx,isx) f_sub(&sr,(s),(d),(m),(sb),((m)==0xFFFFFFFFu?32u:(m)==0xFFFFu?16u:8u),(x),(setx),(isx),(L))
#define CMP(s,d,m,sb) do{ (void)SUBL(15u,(s),(d),(m),(sb),0,0,0); }while(0)
#define CMPL(L,s,d,m,sb) do{ (void)SUBL((L)&15u,(s),(d),(m),(sb),0,0,0); }while(0)
AINL int CC(u32 sr,int c){
  u32 Cf=(sr&FC_)!=0,V=(sr&FV_)!=0,Z=(sr&FZ_)!=0,N=(sr&FN_)!=0;
  switch(c){case 0:return 1;case 1:return 0;case 2:return !Cf&&!Z;case 3:return Cf||Z;case 4:return !Cf;case 5:return (int)Cf;case 6:return !Z;case 7:return (int)Z;
  case 8:return !V;case 9:return (int)V;case 10:return !N;case 11:return (int)N;case 12:return N==V;case 13:return N!=V;case 14:return !Z&&N==V;default:return Z||N!=V;} }
/* shift/rotate, exact port of m68k_ref.c shift() */
static inline u32 f_shift(u32*psr,u32*pcyc,int type,int left,u32 v,int cnt,u32 m,u32 sb){
  u32 sr=*psr; v&=m; sr&=~(FV_|FC_);
  if(cnt==0){ if(type==2){ if(sr&FX_) sr|=FC_; } NZ(v,m,sb); sr=(sr&~FC_)|((type==2&&(sr&FX_))?FC_:0u); *psr=sr; return v; }
  u32 c=0,vf=0, x=(sr&FX_)?1u:0u;
  /* CP16 recovery: replace the MC68000 graphics decoder's repeated
   * logical-shift bit loop with native ARM shifts.  For counts >= operand
   * width the final carry must remain the *last* bit shifted out; counts
   * above the width clear carry.  The common immediate shifts fold fully
   * at compile time in each AOT instruction.  CP27 extends closed forms to arithmetic shifts and ordinary rotates;
   * rotate-through-extend retains the canonical per-bit implementation. */
  if(type==1){
    u32 width=(m==0xFFu)?8u:(m==0xFFFFu)?16u:32u;
    if(left){
      c=(u32)cnt<=width?((v>>(width-(u32)cnt))&1u):0u;
      v=(u32)cnt>=width?0u:(v<<(u32)cnt)&m;
    }else{
      c=(u32)cnt<=width?((v>>((u32)cnt-1u))&1u):0u;
      v=(u32)cnt>=width?0u:v>>(u32)cnt;
    }
    x=c;
  }else if(type==3){
    u32 width=(m==255u)?8u:(m==65535u)?16u:32u,r=(u32)cnt&(width-1u);
    if(r)v=left?((v<<r)|(v>>(width-r)))&m:((v>>r)|(v<<(width-r)))&m;
    c=left?(v&1u):((v&sb)?1u:0u);
  }else if(type==0){
    u32 width=(m==255u)?8u:(m==65535u)?16u:32u,n=(u32)cnt;
    if(left){
      if(n<width){c=(v>>(width-n))&1u;vf=((v^((v&sb)?m:0u))>>(width-n-1u))!=0u;v=(v<<n)&m;}
      else {c=n==width?(v&1u):0u;vf=v!=0u;v=0;}
    }else{
      s32 sv=width==8u?(s32)(s8)v:width==16u?(s32)(s16)v:(s32)v;
      if(n<width){c=(v>>(n-1u))&1u;v=(u32)(sv>>n)&m;}
      else {c=(v&sb)?1u:0u;v=c?m:0u;}
    }
    x=c;
  }else for(int i=0;i<cnt;i++){
    switch(type){
      case 0: if(left){ c=(v&sb)?1u:0u; u32 nv=(v<<1)&m; if((nv&sb)!=(v&sb)) vf=1; v=nv; } else { c=v&1u; v=(v>>1)|(v&sb); } x=c; break;
      case 1: if(left){ c=(v&sb)?1u:0u; v=(v<<1)&m; } else { c=v&1u; v>>=1; } x=c; break;
      case 2: if(left){ c=(v&sb)?1u:0u; v=((v<<1)|x)&m; } else { c=v&1u; v=(v>>1)|(x?sb:0u); } x=c; break;
      default: if(left){ c=(v&sb)?1u:0u; v=((v<<1)|c)&m; } else { c=v&1u; v=(v>>1)|(c?sb:0u); } break;
    } }
  u32 keepx=sr&FX_; NZ(v,m,sb); if(c) sr|=FC_; if(vf) sr|=FV_;
  if(type!=3){ sr&=~FX_; if(x) sr|=FX_; } else sr|=keepx;
  *pcyc+=2u*(u32)cnt; *psr=sr; return v; }
AINL u32 f_bcd_add(u32*psr,u32 s,u32 d){ u32 sr=*psr; int x=(sr&FX_)?1:0; int lo=(int)(s&15)+(int)(d&15)+x; int r=(int)(s&0xF0)+(int)(d&0xF0)+lo; if(lo>9) r+=6; int c=0; if(r>0x99){ r+=0x60; c=1; }
  sr&=~(FC_|FX_); if(c) sr|=FC_|FX_; if(r&0xFF) sr&=~FZ_; *psr=sr; return (u32)(u8)r; }
AINL u32 f_bcd_sub(u32*psr,u32 s,u32 d){ u32 sr=*psr; int x=(sr&FX_)?1:0; int lo=(int)(d&15)-(int)(s&15)-x; int r=(int)(d&0xF0)-(int)(s&0xF0)+lo; if(lo<0) r-=6; int c=0; if(r<0){ r-=0x60; c=1; }
  sr&=~(FC_|FX_); if(c) sr|=FC_|FX_; if(r&0xFF) sr&=~FZ_; *psr=sr; return (u32)(u8)r; }

/* return codes of a translated function */
#define AOT_CONT 0u      /* C.pc set: continue dispatching */
#define AOT_INTERP 1u    /* run the instruction at C.pc with the reference interpreter */
#define AOT_SLOW 2u      /* the block at C.pc could reach the cycle limit: interpret up to the limit */
typedef u32 (*aot_fn)(u32 entry);
#ifdef AOT_ROM2
#define AOT_TAB __attribute__((section(".rom2")))   /* lookup tables live in the erased ROM tail (see gbabuild / build_cp08) */
#else
#define AOT_TAB
#endif
typedef struct { u32 pc; u16 fn, idx, rr, pad; } AotEnt;                 /* fn: index in aot_fns; rr: RAM region, 0xFFFF = ROM */
typedef struct { u16 first, n; } AotRamReg;                              /* ranges (offset, length) in the RAM code window */
typedef struct { u32 index; const AotEnt*rom; u32 nrom; const AotEnt*ram; u32 nram; const u16*rompg; const u16*rampg;
                 const AotRamReg*rr; u32 nrr; const u16*rng; const AotEnt*xrom; u32 nxrom; } AotGame;  /* *pg: 4 KiB page -> aot_pool bitmap (0xFFFF none) */

/* CP18 native 32-bit 68000 decimal DIVU helper; ROM ARM implementation. */
u32 aot_div10_arm(u32 n);
u32 aot_div20_arm(u32 n);

u32 md_native_decode_bits(u32 room,u32 sr,u32 hi);

u32 aot_div18_arm(u32 n);

u32 md_native_ram_bytes(u32 src,u32 dst,u32 n,u32 fill);

u32 aot_div12_arm(u32 n);

u32 md_native_nibble_blend(u32 room,u32 sr);

u32 md_native_kern_scan(u32 room,u32 sr);
u32 md_native_class_scan(u32 room,u32 sr);

u32 md_native_glyph_bytes(u32 room,u32 sr);

u32 md_native_nametable(u32 room,u32 sr);

u32 md_native_pack_tile(u32 room,u32 sr);

/* CP31 native logical shift, specialised by compile-time direction/width/count.
 * 68000 register shifts charge 2 cycles per bit and always update X/C together
 * when count != 0.  Exact semantics of f_shift(type=1,count=1..8).
 */
AINL u32 aot_lshift_imm(u32*psr,u32*pcyc,u32 v,u32 count,u32 mask,u32 sign,u32 left){
  const u32 width=(mask==0xFFu)?8u:(mask==0xFFFFu)?16u:32u;
  v&=mask;
  u32 carry=(left?(v>>(width-count)):(v>>(count-1u)))&1u;
  v=left?((v<<count)&mask):(v>>count);
  *psr=(*psr&~31u)|(carry?17u:0u)|(v?0u:4u)|((v&sign)?8u:0u);
  *pcyc+=2u*count;
  return v;
}

/* CP31: exact 68000 ROXL #1 register, all widths. X and C receive the old sign.
 * The old X is shifted into the low bit, V clears, N/Z come from the new value. */
AINL u32 aot_roxl1(u32*psr,u32*pcyc,u32 v,u32 mask,u32 sign){
  u32 old=*psr;v&=mask;
  u32 carry=(v&sign)?1u:0u;
  v=((v<<1)|((old>>4)&1u))&mask;
  *psr=(old&~31u)|(carry?17u:0u)|(v?0u:4u)|((v&sign)?8u:0u);
  *pcyc+=2u;return v;
}
/* CP31: arithmetic register shift with fixed count (68000 X/C/V semantics). */
AINL u32 aot_ashift_imm(u32*psr,u32*pcyc,u32 v,u32 count,u32 mask,u32 sign,u32 left){
  u32 width=mask==255u?8u:mask==65535u?16u:32u;
  v&=mask;u32 carry,ov=0;
  if(left){
    if(count<width){
      carry=(v>>(width-count))&1u;
      ov=((v^((v&sign)?mask:0u))>>(width-count-1u))!=0u;
      v=(v<<count)&mask;
    }else{carry=count==width?(v&1u):0u;ov=v!=0u;v=0u;}
  }else{
    carry=count<width?((v>>(count-1u))&1u):((v&sign)?1u:0u);
    s32 sv=width==8u?(s32)(s8)v:width==16u?(s32)(s16)v:(s32)v;
    v=count<width?((u32)(sv>>count)&mask):(carry?mask:0u);
  }
  *psr=(*psr&~31u)|(carry?17u:0u)|(ov?2u:0u)|(v?0u:4u)|((v&sign)?8u:0u);
  *pcyc+=2u*count;return v;
}
/* CP31: fixed-count 68000 ROL/ROR, X remains unchanged. */
AINL u32 aot_rotate_imm(u32*psr,u32*pcyc,u32 v,u32 count,u32 mask,u32 sign,u32 left){
  u32 width=mask==255u?8u:mask==65535u?16u:32u,r=count&(width-1u);
  v&=mask;
  if(r)v=left?((v<<r)|(v>>(width-r)))&mask:((v>>r)|(v<<(width-r)))&mask;
  u32 carry=left?(v&1u):((v&sign)?1u:0u);
  *psr=(*psr&~15u)|(carry?1u:0u)|(v?0u:4u)|((v&sign)?8u:0u);
  *pcyc+=2u*count;return v;
}
