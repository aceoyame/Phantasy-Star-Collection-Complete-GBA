/* Fast MC68000 interpreter for the GBA Genesis runtime (CP08).
 *
 * Design:
 *  - One switch on the top opcode nibble.  The most frequent instruction (MOVE)
 *    is specialised per operand size; everything else uses run-time size masks
 *    so each family is compiled once (the fully specialised version was >100 KB;
 *    this one is ~14 KB and has to fit in IWRAM together with the VDP/renderer).
 *  - pc, cycle counter and SR live in locals (registers) for the whole call.
 *  - Instruction fetch is inlined; data accesses and effective-address
 *    calculation are small out-of-line helpers that also live in IWRAM.
 *  - Only well-formed encodings are executed here.  Anything rare, privileged
 *    or malformed jumps to `slow:` which rewinds to the opcode and runs the
 *    reference interpreter (m68k_ref.c) so behaviour stays bit-identical.
 *  - Cycle accounting copies the reference model exactly.
 *  - Taken branches feed a small ring (diagnostics) and an idle-spin detector.
 *
 * tools_host/md_host/fuzz_cpu.c checks this file against m68k_ref.c for all
 * 65,536 opcodes over many random register/SR/memory states.
 */
#include "m68k.h"
#include "md.h"
#ifdef AOT_ON
#define m68k_exec m68k_exec_interp
int aot_has(u32 pc);
#define AOT_LEAVE(t) do{ if(aot_has(t)) limit=cyc; }while(0)
extern u32 aot_rgen[128];
#define RGEN(a) do{ if(((a)&0xFFFFu)<0x8000u) AOT_RGEN_ADD(((a)>>8)&0x7Fu,1u); }while(0)
#else
#define RGEN(a) do{}while(0)
#define AOT_LEAVE(t) do{}while(0)
#endif

M68K C M68K_HOT;
const u8* m68k_romp[128] M68K_HOT;
u32 m68k_romsize M68K_HOT;
u16* m68k_ram M68K_HOT;
static u16 cc_tab[16] M68K_HOT;   /* CCR nibble (N Z V C) -> bitmask of true conditions */
static u32 szm[4] M68K_HOT={0xFFu,0xFFFFu,0xFFFFFFFFu,0u};
unsigned __aeabi_uidiv(unsigned n,unsigned d);

typedef u32 __attribute__((may_alias)) NativeRamWord;
__attribute__((noinline,section(".hot"))) u32 m68k_movem_ram(u32 a,u32 mask,u32 mode){
  u32 off=a&0xFFFFu;
  if((a&0xFFFFFFu)<0xE00000u||(off&1u))return 0;
  /* Stay clear of code generations and all wrapping/bus edge cases. */
  if(mode==2u){if(off<0x8040u)return 0;}
  else if(off<0x8000u||off>0xFFBCu)return 0;
  u16 *p=m68k_ram+(off>>1);
  if(mode==2u&&mask==0xFFFEu){
    for(u32 i=15u;i--;){u32 v=C.regs[i];p-=2;p[0]=(u16)(v>>16);p[1]=(u16)v;}
    C.fx+=15u;return 15u;
  }
  if(mode==1u&&mask==0x7FFFu){
    for(u32 i=0;i<15u;i++){C.regs[i]=((u32)p[0]<<16)|p[1];p+=2;}
    return 15u;
  }
  u32 n=0;
  for(u32 bit=0;bit<16u;bit++)if(mask&(1u<<bit)){
    u32 i=mode==2u?15u-bit:bit;
    if(mode==1u){C.regs[i]=((u32)p[0]<<16)|p[1];p+=2;}
    else {u32 v=C.regs[i];if(mode==2u)p-=2;p[0]=(u16)(v>>16);p[1]=(u16)v;if(mode!=2u)p+=2;}
    n++;
  }
  if(mode!=1u)C.fx+=n;
  return n;
}

#define F_C 1u
#define F_V 2u
#define F_Z 4u
#define F_N 8u
#define F_X 16u

#define MASK(SZ) ((SZ)==1?0xFFu:(SZ)==2?0xFFFFu:0xFFFFFFFFu)
#define INL static inline __attribute__((always_inline))
#define NOINL static __attribute__((noinline))

/* ---------------- bus fast paths ----------------
 * Instruction fetch is inlined (ROM first).  Data reads/writes are out-of-line
 * helpers. */
INL u32 FETCH16(u32 a){
  a&=0xFFFFFEu;
  if(a<m68k_romsize){ u32 h=*(const u16*)(m68k_romp[a>>12]+(a&0xFFFu)); return ((h&0xFFu)<<8)|(h>>8); }
  if(a>=0xE00000u) return m68k_ram[(a&0xFFFFu)>>1];
  return bus_read16(a);
}
__attribute__((noinline,section(".hot"))) u32 RD8(u32 a){
  a&=0xFFFFFFu;
  if(a<m68k_romsize) return m68k_romp[a>>12][a&0xFFFu];
  if(a>=0xE00000u) return ((const u8*)m68k_ram)[(a&0xFFFFu)^1u];
  return bus_read8(a);
}
__attribute__((noinline,section(".hot"))) u32 RD16(u32 a){
  a&=0xFFFFFEu;
  if(a<m68k_romsize){ u32 h=*(const u16*)(m68k_romp[a>>12]+(a&0xFFFu)); return ((h&0xFFu)<<8)|(h>>8); }
  if(a>=0xE00000u) return m68k_ram[(a&0xFFFFu)>>1];
  return bus_read16(a);
}
__attribute__((noinline,section(".hot"))) u32 RD32(u32 a){
  a&=0xFFFFFEu;
  if(a<m68k_romsize&&(a&0xFFFu)<0xFFCu){ const u8*p=m68k_romp[a>>12]+(a&0xFFFu); u32 hi=*(const u16*)p,lo=*(const u16*)(p+2); return ((hi&0xFFu)<<24)|((hi>>8)<<16)|((lo&0xFFu)<<8)|(lo>>8); }
  if(a>=0xE00000u&&(a&0xFFFFu)<0xFFFCu){ const u16*p=m68k_ram+((a&0xFFFFu)>>1); return ((u32)p[0]<<16)|p[1]; }
  u32 hi=RD16(a); return (hi<<16)|RD16(a+2u);
}
__attribute__((noinline,section(".hot"))) void WR8(u32 a,u32 v){
  a&=0xFFFFFFu;
  if(a>=0xE00000u){ ((u8*)m68k_ram)[(a&0xFFFFu)^1u]=(u8)v; C.fx++; RGEN(a); return; }
  bus_write8(a,(u8)v);
}
__attribute__((noinline,section(".hot"))) void WR16(u32 a,u32 v){
  a&=0xFFFFFEu;
  if(a>=0xE00000u){ m68k_ram[(a&0xFFFFu)>>1]=(u16)v; C.fx++; RGEN(a); return; }
  if((a&0xE700FCu)==0xC00000u){ md_vdp_write16(v); return; }
  bus_write16(a,(u16)v);
}
__attribute__((noinline,section(".hot"))) void WR32(u32 a,u32 v){
  a&=0xFFFFFEu;
  if(a>=0xE00000u&&(a&0xFFFFu)<0xFFFCu){ u16*p=m68k_ram+((a&0xFFFFu)>>1); p[0]=(u16)(v>>16); p[1]=(u16)v; C.fx++; RGEN(a); RGEN(a+2u); return; }
  if((a&0xE700FEu)==0xC00000u){ md_vdp_write32(v); return; }
  WR16(a,v>>16); WR16(a+2u,v);
}
/* Run-time-size access (sz is 1, 2 or 4). */
#define RDV(sz,a) ((sz)==2u?RD16(a):(sz)==4u?RD32(a):RD8(a))
#define WRV(sz,a,v) do{ if((sz)==2u)WR16(a,v); else if((sz)==4u)WR32(a,v); else WR8(a,v); }while(0)
#define RDSZ(SZ,a) ((SZ)==1?RD8(a):(SZ)==2?RD16(a):RD32(a))
#define WRSZ(SZ,a,v) do{ if((SZ)==1)WR8(a,v); else if((SZ)==2)WR16(a,v); else WR32(a,v); }while(0)
#define MEMC(sz) ((sz)==4u?8u:4u)

/* ---------------- effective addresses ---------------- */
/* Valid-source test: everything except mode 7 regs 5..7. */
#define SRC_OK(m,r) ((m)<7u||(r)<=4u)
/* Valid-destination: alterable memory/data modes only. */
#define DST_OK(m,r) ((m)<7u||(r)<=1u)

/* Computes an address for memory modes 2..7 (7/4 immediate excluded).
 * Updates An for (An)+/-(An), and returns the address in the low word and
 * (extension bytes consumed | extra cycles<<8) in the high word, adding the
 * reference model's EA cycles.  Out-of-line: it is used at ~60 sites. */
NOINL u64 ea_calc(u32 mode,u32 reg,u32 sz,u32 pc,u32*A,const u32*D){
  u32 a,adv=0,cy=0;
  switch(mode){
    case 2: a=A[reg]; break;
    case 3: a=A[reg]; A[reg]=a+((reg==7u&&sz==1u)?2u:sz); break;
    case 4: A[reg]-=(reg==7u&&sz==1u)?2u:sz; a=A[reg]; cy=2u; break;
    case 5: a=A[reg]+(u32)(s32)(s16)FETCH16(pc); adv=2u; cy=4u; break;
    case 6: { u32 e=FETCH16(pc); adv=2u; u32 r=(e>>12)&7u,x;
      if(e&0x8000u) x=(e&0x800u)?A[r]:(u32)(s32)(s16)A[r]; else x=(e&0x800u)?D[r]:(u32)(s32)(s16)D[r];
      a=A[reg]+x+(u32)(s32)(s8)e; cy=6u; break; }
    default:
      switch(reg){
        case 0: a=(u32)(s32)(s16)FETCH16(pc); adv=2u; cy=4u; break;
        case 1: a=RD32(pc); adv=4u; cy=8u; break;
        case 2: a=pc+(u32)(s32)(s16)FETCH16(pc); adv=2u; cy=4u; break;
        default: { u32 e=FETCH16(pc); u32 b=pc; adv=2u; u32 r=(e>>12)&7u,x;
          if(e&0x8000u) x=(e&0x800u)?A[r]:(u32)(s32)(s16)A[r]; else x=(e&0x800u)?D[r]:(u32)(s32)(s16)D[r];
          a=b+x+(u32)(s32)(s8)e; cy=6u; break; }
      }
  }
  return (u64)a|((u64)(adv|(cy<<8))<<32);
}
#define EAADDR(mode,reg,sz) ({ u64 _r=ea_calc(mode,reg,sz,pc,A,D); u32 _h=(u32)(_r>>32); pc+=_h&0xFFu; cyc+=_h>>8; (u32)_r; })

/* Source operand fetch (value masked to size).  Matches ea_get+ea_rd.
 * Compile-time size variant (MOVE) and run-time size variant. */
#define LOADSRC(v,SZ,mode,reg) do{ \
  if((mode)==0u) (v)=D[reg]&MASK(SZ); \
  else if((mode)==1u) (v)=A[reg]&MASK(SZ); \
  else if((mode)==7u&&(reg)==4u){ if((SZ)==4){ (v)=RD32(pc); pc+=4u; } else { (v)=FETCH16(pc)&MASK(SZ); pc+=2u; } cyc+=4u; } \
  else { u32 _a=EAADDR(mode,reg,SZ); cyc+=((SZ)==4?8u:4u); (v)=RDSZ(SZ,_a); } }while(0)
#define LOADSRCV(v,sz,mask,mode,reg) do{ \
  if((mode)==0u) (v)=D[reg]&(mask); \
  else if((mode)==1u) (v)=A[reg]&(mask); \
  else if((mode)==7u&&(reg)==4u){ if((sz)==4u){ (v)=RD32(pc); pc+=4u; } else { (v)=FETCH16(pc)&(mask); pc+=2u; } cyc+=4u; } \
  else { u32 _a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); (v)=RDV(sz,_a); } }while(0)

/* ---------------- flags (run-time mask/sign) ---------------- */
#define SETNZ(r) do{ u32 _r=(r)&mask; sr=(sr&~(F_N|F_Z|F_V|F_C))|(_r==0u?F_Z:0u)|((_r&sign)?F_N:0u); }while(0)
#define SETNZ32(r) do{ u32 _r=(r); sr=(sr&~(F_N|F_Z|F_V|F_C))|(_r==0u?F_Z:0u)|((_r&0x80000000u)?F_N:0u); }while(0)
#define SETNZSZ(r,SZ) do{ u32 _r=(r)&MASK(SZ); sr=(sr&~(F_N|F_Z|F_V|F_C))|(_r==0u?F_Z:0u)|((_r&(MASK(SZ)^(MASK(SZ)>>1)))?F_N:0u); }while(0)

/* ADD: result in out, flags (X too when setx). */
#define G_ADD(s,d,out,setx) do{ u32 _s=(s)&mask,_d=(d)&mask,_r=(_s+_d)&mask; \
  u32 _c=((_s&_d)|((_s|_d)&~_r))&sign, _v=(~(_s^_d)&(_s^_r))&sign; \
  sr=(sr&~((setx)?(F_N|F_Z|F_V|F_C|F_X):(F_N|F_Z|F_V|F_C)))|(_c?((setx)?(F_C|F_X):F_C):0u)|(_v?F_V:0u)|((_r&sign)?F_N:0u)|(_r==0u?F_Z:0u); (out)=_r; }while(0)
/* SUB: d - s */
#define G_SUB(s,d,out,setx) do{ u32 _s=(s)&mask,_d=(d)&mask,_r=(_d-_s)&mask; \
  u32 _c=((~_d&_s)|((~_d|_s)&_r))&sign, _v=((_s^_d)&(_d^_r))&sign; \
  sr=(sr&~((setx)?(F_N|F_Z|F_V|F_C|F_X):(F_N|F_Z|F_V|F_C)))|(_c?((setx)?(F_C|F_X):F_C):0u)|(_v?F_V:0u)|((_r&sign)?F_N:0u)|(_r==0u?F_Z:0u); (out)=_r; }while(0)
/* CMP: flags of d - s, X preserved. */
#define G_CMP(s,d) do{ u32 _o; G_SUB(s,d,_o,0); (void)_o; }while(0)

#define COND(c) ((cc_tab[sr&15u]>>(c))&1u)
#define PARAMS(szc) u32 mask=szm[szc],sign=mask^(mask>>1),sz=1u<<(szc); (void)sign; (void)sz

/* ---------------- loop/branch bookkeeping ---------------- */
/* Repeats of the previous entry (idle loops) are not logged, so the ring keeps real history. */
#define RING(kind,t) do{ u32 _e=((u32)(kind)<<24)|((t)&0xFFFFFFu); if(C.ring[(C.ring_pos-1u)&(M68K_RING-1u)]!=_e) C.ring[C.ring_pos++&(M68K_RING-1u)]=_e; }while(0)

static inline int regs_same(const u32*D,const u32*A,u32 sr){
  const u32*s=C.spin_regs;
  return D[0]==s[0]&&D[1]==s[1]&&D[2]==s[2]&&D[3]==s[3]&&D[4]==s[4]&&D[5]==s[5]&&D[6]==s[6]&&D[7]==s[7]&&
         A[0]==s[8]&&A[1]==s[9]&&A[2]==s[10]&&A[3]==s[11]&&A[4]==s[12]&&A[5]==s[13]&&A[6]==s[14]&&A[7]==s[15]&&sr==s[16];
}
static inline void regs_save(const u32*D,const u32*A,u32 sr){
  u32*s=C.spin_regs; for(unsigned i=0;i<8;i++){ s[i]=D[i]; s[8+i]=A[i]; } s[16]=sr;
}

/* n/d for the loop fast-forward: ceil(2^32/d) cached per loop length, exact for n<2^20, d<2^12 (n*d<2^32). */
static inline u32 spin_div(u32 n,u32 d){
  if(d<2u||d>=4096u||n>=0x100000u) return d<2u?n:__aeabi_uidiv(n,d);
  if(d!=C.rcp_per){ C.rcp=__aeabi_uidiv(0xFFFFFFFFu,d)+1u; C.rcp_per=d; }
  return (u32)(((uint64_t)n*C.rcp)>>32);
}

/* Idle-loop detection, called for every taken backward branch (target t); returns the (possibly advanced) cycle count.
 * Two consecutive taken backward branches to the same target with identical registers and no side effect (fx) in
 * between mean the loop is a pure function of its state, so whole iterations can be skipped.  The detector stays
 * armed after a skip (the state at the iteration start is unchanged), so the next scanline - or a multi-scanline
 * chunk, see md_run - folds immediately.  Out of line: it is used from every branch site. */
static __attribute__((noinline)) u32 spin_note(u32 t,u32 limit,u32 cyc,u32 sr){
  const u32*D=C.d,*A=C.a;
  if(C.spin_valid){
    if(regs_same(D,A,sr)){
      s32 rem=(s32)(limit-cyc); u32 per=cyc-C.spin_cyc;
      if(rem>0&&per>0u&&(u32)rem>=per){ u32 d=spin_div((u32)rem,per)*per; cyc+=d; C.spin_skips++; C.spin_cycles+=d; C.idle_ok=(C.lrd==C.spin_lrd); }
      C.spin_back=1; C.spin_cyc=cyc; C.spin_lrd=C.lrd;
    } else {                                  /* registers still changing: not an idle loop (yet); look again after a few iterations */
      C.spin_valid=0; C.spin_wait=C.spin_back; if(C.spin_back<8u) C.spin_back<<=1;
    }
  } else { regs_save(D,A,sr); C.spin_cyc=cyc; C.spin_lrd=C.lrd; C.spin_valid=1; }
  (void)t;
  return cyc;
}

/* Taken-branch bookkeeping; target t is where execution continues. */
#define TAKEN(kind,t) do{ u32 _t=(t); RING(kind,_t); \
  if(_t<pc&&C.spin_enable){ \
    if(_t==C.spin_tgt&&C.fx==C.spin_fx){ if(C.spin_wait) C.spin_wait--; else cyc=spin_note(_t,limit,cyc,sr); } \
    else { C.spin_tgt=_t; C.spin_fx=C.fx; C.spin_valid=0; } } \
  pc=_t; AOT_LEAVE(_t); }while(0)

/* ---------------- shifts and rotates (closed forms, reference-exact) ----------------
 * type: 0 AS 1 LS 2 ROX 3 RO.  Only LS, RO and ASR are done here; others use slow.
 * Returns result in the low word and the new SR in the high word. */
#define SHIFT_FAST_OK(type,left) ((type)==1u||(type)==3u||((type)==0u&&!(left)))
__attribute__((section(".text.rom"))) NOINL u64 do_shift(u32 v,u32 cnt,u32 mask,u32 type,u32 left,u32 sr){
  u32 sign=mask^(mask>>1),bits=(mask==0xFFu)?8u:(mask==0xFFFFu)?16u:32u,res,c=0;
  v&=mask;
  if(cnt==0u){ res=v; sr=(sr&~(F_N|F_Z|F_V|F_C))|(res==0u?F_Z:0u)|((res&sign)?F_N:0u); return (u64)res|((u64)sr<<32); }
  if(type==3u){ u32 r=cnt&(bits-1u);
    if(left){ res=r?(((v<<r)|(v>>(bits-r)))&mask):v; c=res&1u; }
    else { res=r?(((v>>r)|(v<<(bits-r)))&mask):v; c=(res&sign)?1u:0u; } }
  else if(type==1u){
    if(left){ if(cnt<bits){ res=(v<<cnt)&mask; c=(v>>(bits-cnt))&1u; } else { res=0u; c=(cnt==bits)?(v&1u):0u; } }
    else { if(cnt<bits){ res=v>>cnt; c=(v>>(cnt-1u))&1u; } else { res=0u; c=(cnt==bits)?((v>>(bits-1u))&1u):0u; } } }
  else { /* ASR */ s32 sv=bits==8u?(s32)(s8)v:bits==16u?(s32)(s16)v:(s32)v;
    if(cnt<bits){ res=(u32)(sv>>cnt)&mask; c=(v>>(cnt-1u))&1u; } else { res=(sv<0)?mask:0u; c=(sv<0)?1u:0u; } }
  sr=(sr&~(F_N|F_Z|F_V|F_C))|(res==0u?F_Z:0u)|((res&sign)?F_N:0u)|(c?F_C:0u);
  if(type!=3u) sr=(sr&~F_X)|(c?F_X:0u);
  return (u64)res|((u64)sr<<32);
}

__attribute__((section(".text.rom"))) void m68k_init_tables(void){
  for(unsigned f=0;f<16;f++){
    unsigned c=(f&1)!=0,v=(f&2)!=0,z=(f&4)!=0,n=(f&8)!=0,m=0;
    unsigned t[16]={1,0,(!c&&!z),(c||z),!c,c,!z,z,!v,v,!n,n,n==v,n!=v,(!z&&n==v),(z||n!=v)};
    for(unsigned i=0;i<16;i++) if(t[i]) m|=1u<<i;
    cc_tab[f]=(u16)m;
  }
}

/* CP11: the interpreter remains available in ROM. AOT handles the hot game paths;
 * its former IWRAM footprint is used for the sample-identical sound renderer. */
__attribute__((section(".text.rom"))) void m68k_exec(u32 limit){
  u32 pc=C.pc,cyc=C.cycles,sr=C.sr;
  u32 pt=~0u; const u8*pp=0;                 /* cached code page (ROM pages never change) */
  C.idle_ok=0;
  u32*const D=C.d; u32*const A=C.a;
  do{
    u32 op;
    { u32 t=pc>>12;
      if(t==pt){ u32 h=*(const u16*)(pp+(pc&0xFFEu)); op=((h&0xFFu)<<8)|(h>>8); }
      else { op=FETCH16(pc); if(t<(m68k_romsize>>12)){ pt=t; pp=m68k_romp[t]; } } }
    pc+=2u; cyc+=4u;
    switch(op>>12){

    /* ================= MOVE / MOVEA ================= */
#define MOVE_BODY(SZ) { \
      u32 sm=(op>>3)&7u,sr_=op&7u,dm=(op>>6)&7u,dr=(op>>9)&7u,v; \
      if(!SRC_OK(sm,sr_)) goto slow; \
      if(dm==1u){ if((SZ)==1u||(sm==1u&&(SZ)==1u)) goto slow; LOADSRC(v,SZ,sm,sr_); A[dr]=((SZ)==2)?(u32)(s32)(s16)v:v; break; } \
      if(!DST_OK(dm,dr)) goto slow; \
      if((SZ)==1u&&sm==1u) goto slow; \
      LOADSRC(v,SZ,sm,sr_); \
      if(dm==0u){ if((SZ)==4) D[dr]=v; else D[dr]=(D[dr]&~MASK(SZ))|v; } \
      else { u32 _a=EAADDR(dm,dr,SZ); cyc+=((SZ)==4?8u:4u); WRSZ(SZ,_a,v); } \
      SETNZSZ(v,SZ); break; }
    case 1: MOVE_BODY(1)
    case 3: MOVE_BODY(2)
    case 2: MOVE_BODY(4)

    /* ================= line 0: immediate ops and bit ops ================= */
    case 0: {
      u32 mode=(op>>3)&7u,reg=op&7u;
      if(op&0x100u){
        if(mode==1u) goto slow;                       /* MOVEP */
        u32 t=(op>>6)&3u,bit=D[(op>>9)&7u];
        if(mode==0u){ bit&=31u; u32 v=D[reg]; sr=(v&(1u<<bit))?(sr&~F_Z):(sr|F_Z);
          if(t==1u) D[reg]=v^(1u<<bit); else if(t==2u) D[reg]=v&~(1u<<bit); else if(t==3u) D[reg]=v|(1u<<bit); break; }
        if(!DST_OK(mode,reg)) goto slow;
        bit&=7u; u32 a=EAADDR(mode,reg,1); cyc+=4u; u32 v=RD8(a); sr=(v&(1u<<bit))?(sr&~F_Z):(sr|F_Z);
        if(t){ cyc+=4u; if(t==1u) WR8(a,v^(1u<<bit)); else if(t==2u) WR8(a,v&~(1u<<bit)); else WR8(a,v|(1u<<bit)); }
        break;
      }
      u32 which=(op>>9)&7u;
      if(which==4u){
        u32 t=(op>>6)&3u; u32 bit=FETCH16(pc)&0xFFu; pc+=2u;
        if(mode==0u){ bit&=31u; u32 v=D[reg]; sr=(v&(1u<<bit))?(sr&~F_Z):(sr|F_Z);
          if(t==1u) D[reg]=v^(1u<<bit); else if(t==2u) D[reg]=v&~(1u<<bit); else if(t==3u) D[reg]=v|(1u<<bit); break; }
        if(mode==1u||!DST_OK(mode,reg)){ pc-=2u; goto slow; }
        bit&=7u; u32 a=EAADDR(mode,reg,1); cyc+=4u; u32 v=RD8(a); sr=(v&(1u<<bit))?(sr&~F_Z):(sr|F_Z);
        if(t){ cyc+=4u; if(t==1u) WR8(a,v^(1u<<bit)); else if(t==2u) WR8(a,v&~(1u<<bit)); else WR8(a,v|(1u<<bit)); }
        break;
      }
      u32 szc=(op>>6)&3u; if(szc==3u||which==7u) goto slow;
      if(mode==1u||(mode==7u&&reg>=2u)) goto slow;    /* An, CCR/SR forms, PC-relative */
      { PARAMS(szc); u32 imm,d,r,a=0;
        if(sz==4u){ imm=RD32(pc); pc+=4u; } else { imm=FETCH16(pc)&mask; pc+=2u; }
        if(mode==0u) d=D[reg]&mask; else { a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); d=RDV(sz,a); }
        switch(which){
          case 0: r=d|imm; SETNZ(r); break;
          case 1: r=d&imm; SETNZ(r); break;
          case 2: G_SUB(imm,d,r,1); break;
          case 3: G_ADD(imm,d,r,1); break;
          case 5: r=d^imm; SETNZ(r); break;
          default: G_CMP(imm,d); goto next_insn; }
        if(mode==0u) D[reg]=(D[reg]&~mask)|(r&mask); else { cyc+=MEMC(sz); WRV(sz,a,r); } }
      break;
    }

    /* ================= line 4 ================= */
    case 4: {
      u32 mode=(op>>3)&7u,reg=op&7u;
      u32 ctl=(mode==2u||mode==5u||mode==6u||(mode==7u&&reg<=3u));   /* control addressing modes */
      if((op&0xF1C0u)==0x41C0u){ if(!ctl) goto slow; A[(op>>9)&7u]=EAADDR(mode,reg,4); break; }   /* LEA */
      if((op&0xF1C0u)==0x4180u) goto slow;                                                          /* CHK */
      switch(op&0xFFC0u){
        case 0x40C0u: /* MOVE from SR */
          if(mode==1u||!DST_OK(mode,reg)) goto slow;
          if(mode==0u) D[reg]=(D[reg]&0xFFFF0000u)|(sr&0xFFFFu); else { u32 a=EAADDR(mode,reg,2); cyc+=4u; WR16(a,sr); }
          goto next_insn;
        case 0x44C0u: { /* MOVE to CCR */
          if(mode==1u||!SRC_OK(mode,reg)) goto slow;
          u32 v; LOADSRC(v,2,mode,reg); sr=(sr&0xFF00u)|(v&0x1Fu); goto next_insn; }
        case 0x4840u: /* SWAP / PEA */
          if(mode==0u){ u32 v=D[reg]; v=(v>>16)|(v<<16); D[reg]=v; SETNZ32(v); goto next_insn; }
          if(!ctl) goto slow;
          { u32 a=EAADDR(mode,reg,4); A[7]-=4u; WR32(A[7],a); goto next_insn; }
        case 0x4880u: /* EXT.W / MOVEM.W regs->mem */
          if(mode==0u){ u32 v=(u32)(s32)(s8)D[reg]; D[reg]=(D[reg]&0xFFFF0000u)|(v&0xFFFFu); SETNZSZ(v,2); goto next_insn; }
          goto movem;
        case 0x48C0u: /* EXT.L / MOVEM.L regs->mem */
          if(mode==0u){ u32 v=(u32)(s32)(s16)D[reg]; D[reg]=v; SETNZ32(v); goto next_insn; }
          goto movem;
        case 0x4C80u: case 0x4CC0u: goto movem;
        case 0x4E80u: /* JSR */
          if(!ctl) goto slow;
          { u32 a=EAADDR(mode,reg,4); A[7]-=4u; WR32(A[7],pc); cyc+=8u; TAKEN(RK_JSR,a); goto next_insn; }
        case 0x4EC0u: /* JMP */
          if(!ctl) goto slow;
          { u32 a=EAADDR(mode,reg,4); TAKEN(RK_JMP,a); goto next_insn; }
        default: break;
      }
      { u32 hi=op&0xFF00u;
        if(hi==0x4200u||hi==0x4400u||hi==0x4600u||hi==0x4A00u){
          u32 szc=(op>>6)&3u; if(szc==3u) goto slow;
          if(mode==1u||!DST_OK(mode,reg)) goto slow;
          PARAMS(szc); u32 v,r,a=0;
          if(mode==0u) v=D[reg]&mask; else { a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); v=RDV(sz,a); }
          if(hi==0x4200u){ r=0u; sr=(sr&~(F_N|F_V|F_C))|F_Z; }
          else if(hi==0x4400u){ G_SUB(v,0u,r,1); }
          else if(hi==0x4600u){ r=~v; SETNZ(r); }
          else { SETNZ(v); goto next_insn; }
          if(mode==0u) D[reg]=(D[reg]&~mask)|(r&mask); else { cyc+=MEMC(sz); WRV(sz,a,r); }
          goto next_insn;
        }
      }
      if((op&0xFFF8u)==0x4E50u){ u32 lv=A[reg]; A[7]-=4u; WR32(A[7],lv); A[reg]=A[7]; A[7]+=(u32)(s32)(s16)FETCH16(pc); pc+=2u; break; }  /* LINK */
      if((op&0xFFF8u)==0x4E58u){ A[7]=A[reg]; u32 v=RD32(A[7]); A[7]+=4u; A[reg]=v; break; }                                  /* UNLK */
      if(op==0x4E71u) break;                                                                                                   /* NOP */
      if(op==0x4E75u){ u32 t=RD32(A[7]); A[7]+=4u; cyc+=8u; TAKEN(RK_RET,t); break; }                                          /* RTS */
      goto slow;

      movem: {
        u32 dir=(op>>10)&1u,sz=(op&0x40u)?4u:2u; u32 mask=FETCH16(pc); pc+=2u;
        if(dir==0u&&mode==4u){
          u32 a=A[reg];
          if(sz==4u){u32 n=m68k_movem_ram(a,mask,2u);if(n){A[reg]=a-4u*n;cyc+=8u*n;break;}}
          for(int i=15;i>=0;i--) if(mask&(1u<<(15-i))){ a-=sz; u32 v=i<8?D[i]:A[i-8]; if(sz==4u) WR32(a,v); else WR16(a,v); cyc+=sz==4u?8u:4u; }
          A[reg]=a; break;
        }
        if(dir==0u?!(mode==2u||mode==5u||mode==6u||(mode==7u&&reg<=1u)):!(mode==2u||mode==3u||mode==5u||mode==6u||(mode==7u&&reg<=3u))){ pc-=2u; goto slow; }
        u32 a; if(mode==3u) a=A[reg]; else a=EAADDR(mode,reg,sz);
        if(sz==4u){u32 n=m68k_movem_ram(a,mask,dir);if(n){if(mode==3u)A[reg]=a+4u*n;cyc+=8u*n;break;}}
        for(unsigned i=0;i<16;i++) if(mask&(1u<<i)){
          if(dir){ u32 v=sz==4u?RD32(a):(u32)(s32)(s16)RD16(a); if(i<8) D[i]=v; else A[i-8]=v; }
          else { u32 v=i<8?D[i]:A[i-8]; if(sz==4u) WR32(a,v); else WR16(a,v); }
          a+=sz; cyc+=sz==4u?8u:4u;
        }
        if(mode==3u) A[reg]=a;
        break;
      }
    }

    /* ================= line 5: ADDQ/SUBQ/Scc/DBcc ================= */
    case 5: {
      u32 mode=(op>>3)&7u,reg=op&7u;
      if(((op>>6)&3u)==3u){
        u32 c=(op>>8)&15u;
        if(mode==1u){ /* DBcc */
          u32 dpc=pc; s32 disp=(s32)(s16)FETCH16(pc); pc+=2u;
          if(!COND(c)){
            u32 w=(D[reg]-1u)&0xFFFFu; D[reg]=(D[reg]&0xFFFF0000u)|w;
            if(w!=0xFFFFu){
              if(disp==-2&&c==1u&&C.spin_enable){            /* "DBF Dn,*" delay loop: run it in one step */
                s32 rem=(s32)(limit-cyc);
                if(rem>0){
                  u32 need=((u32)rem+3u)>>2,left=w+1u,k=need<left?need:left;
                  cyc+=4u*k; C.spin_skips++; C.spin_cycles+=4u*k;
                  if(k==left) D[reg]|=0xFFFFu; else { D[reg]=(D[reg]&0xFFFF0000u)|(w-k); pc=dpc+(u32)disp; }
                  break;
                }
              }
              TAKEN(RK_DBF,dpc+(u32)disp);
            }
          }
          break;
        }
        if(!DST_OK(mode,reg)||mode==1u) goto slow;
        { u32 v=COND(c)?0xFFu:0u;
          if(mode==0u) D[reg]=(D[reg]&~0xFFu)|v; else { u32 a=EAADDR(mode,reg,1); cyc+=4u; WR8(a,v); } }
        break;
      }
      u32 szc=(op>>6)&3u,q=(op>>9)&7u; if(!q) q=8u;
      if(mode==1u){ if(szc==0u) goto slow; if(op&0x100u) A[reg]-=q; else A[reg]+=q; break; }
      if(!DST_OK(mode,reg)) goto slow;
      { PARAMS(szc); u32 d,r,a=0;
        if(mode==0u) d=D[reg]&mask; else { a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); d=RDV(sz,a); }
        if(op&0x100u){ G_SUB(q,d,r,1); } else { G_ADD(q,d,r,1); }
        if(mode==0u) D[reg]=(D[reg]&~mask)|r; else { cyc+=MEMC(sz); WRV(sz,a,r); } }
      break;
    }

    /* ================= line 6: branches ================= */
    case 6: {
      u32 c=(op>>8)&15u; s32 disp=(s32)(s8)op; u32 base=pc;
      if(disp==0){ disp=(s32)(s16)FETCH16(pc); pc+=2u; }
      if(c==1u){ A[7]-=4u; WR32(A[7],pc); cyc+=8u; TAKEN(RK_JSR,base+(u32)disp); break; }
      if(COND(c)){ cyc+=2u; TAKEN(RK_BRA,base+(u32)disp); }
      break;
    }

    /* ================= MOVEQ ================= */
    case 7: { if(op&0x100u) goto slow; u32 v=(u32)(s32)(s8)op; D[(op>>9)&7u]=v; SETNZ32(v); break; }

    /* ================= line 8: OR ================= */
    case 8: {
      u32 dr=(op>>9)&7u,mode=(op>>3)&7u,reg=op&7u,opm=(op>>6)&7u;
      if(opm==3u||opm==7u) goto slow;                       /* DIVU/DIVS */
      if((op&0x1F0u)==0x100u) goto slow;                    /* SBCD */
      u32 szc=opm&3u;
      if(opm<3u){ if(!SRC_OK(mode,reg)||mode==1u) goto slow; }
      else { if(!DST_OK(mode,reg)||mode==0u||mode==1u) goto slow; }
      { PARAMS(szc); u32 s,r;
        if(opm<3u){ LOADSRCV(s,sz,mask,mode,reg); r=(D[dr]|s)&mask; SETNZ(r); D[dr]=(D[dr]&~mask)|r; }
        else { u32 a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); s=RDV(sz,a); r=(D[dr]|s)&mask; SETNZ(r); cyc+=MEMC(sz); WRV(sz,a,r); } }
      break;
    }

    /* ================= line 9 / D: SUB / ADD ================= */
    case 9: case 13: {
      u32 sub=((op>>12)==9u);
      u32 dr=(op>>9)&7u,mode=(op>>3)&7u,reg=op&7u,opm=(op>>6)&7u;
      if(opm==3u||opm==7u){ u32 s; if(!SRC_OK(mode,reg)) goto slow;
        if(opm==3u){ LOADSRC(s,2,mode,reg); s=(u32)(s32)(s16)s; } else { LOADSRC(s,4,mode,reg); }
        if(sub) A[dr]-=s; else A[dr]+=s; break; }
      if(opm>=4u&&(mode==0u||mode==1u)) goto slow;
      u32 szc=opm&3u;
      if(opm<3u){ if(!SRC_OK(mode,reg)||(szc==0u&&mode==1u)) goto slow; }
      else { if(!DST_OK(mode,reg)) goto slow; }
      { PARAMS(szc); u32 s,d,r;
        if(opm<3u){ LOADSRCV(s,sz,mask,mode,reg); d=D[dr]&mask; if(sub){ G_SUB(s,d,r,1); } else { G_ADD(s,d,r,1); } D[dr]=(D[dr]&~mask)|r; }
        else { u32 a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); d=RDV(sz,a); s=D[dr]; if(sub){ G_SUB(s,d,r,1); } else { G_ADD(s,d,r,1); } cyc+=MEMC(sz); WRV(sz,a,r); } }
      break;
    }

    /* ================= line B: CMP / CMPA / CMPM / EOR ================= */
    case 11: {
      u32 dr=(op>>9)&7u,mode=(op>>3)&7u,reg=op&7u,opm=(op>>6)&7u;
      if(opm==3u||opm==7u){ u32 s,mask=0xFFFFFFFFu,sign=0x80000000u; if(!SRC_OK(mode,reg)) goto slow;
        if(opm==3u){ LOADSRC(s,2,mode,reg); s=(u32)(s32)(s16)s; } else { LOADSRC(s,4,mode,reg); }
        G_CMP(s,A[dr]); break; }
      u32 szc=opm&3u;
      if(opm<3u){ if(!SRC_OK(mode,reg)||(szc==0u&&mode==1u)) goto slow;
        { PARAMS(szc); u32 s; LOADSRCV(s,sz,mask,mode,reg); G_CMP(s,D[dr]); }
        break; }
      if(mode==1u) goto slow;                                /* CMPM */
      if(!DST_OK(mode,reg)) goto slow;
      { PARAMS(szc); u32 r,a=0,d; if(mode==0u) d=D[reg]&mask; else { a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); d=RDV(sz,a); }
        r=(d^D[dr])&mask; SETNZ(r); if(mode==0u) D[reg]=(D[reg]&~mask)|r; else { cyc+=MEMC(sz); WRV(sz,a,r); } }
      break;
    }

    /* ================= line C: AND / MULU / MULS / EXG ================= */
    case 12: {
      u32 dr=(op>>9)&7u,mode=(op>>3)&7u,reg=op&7u,opm=(op>>6)&7u;
      if(opm==3u||opm==7u){ if(!SRC_OK(mode,reg)||mode==1u) goto slow; u32 s,r; LOADSRC(s,2,mode,reg);
        if(opm==3u) r=(D[dr]&0xFFFFu)*(s&0xFFFFu); else r=(u32)((s32)(s16)D[dr]*(s32)(s16)s);
        D[dr]=r; SETNZ32(r); cyc+=50u; break; }
      if((op&0x1F0u)==0x100u) goto slow;                     /* ABCD */
      if((op&0x1F8u)==0x140u){ u32 t=D[dr]; D[dr]=D[reg]; D[reg]=t; break; }
      if((op&0x1F8u)==0x148u){ u32 t=A[dr]; A[dr]=A[reg]; A[reg]=t; break; }
      if((op&0x1F8u)==0x188u){ u32 t=D[dr]; D[dr]=A[reg]; A[reg]=t; break; }
      u32 szc=opm&3u;
      if(opm<3u){ if(!SRC_OK(mode,reg)||mode==1u) goto slow; } else { if(!DST_OK(mode,reg)||mode==0u||mode==1u) goto slow; }
      { PARAMS(szc); u32 s,r;
        if(opm<3u){ LOADSRCV(s,sz,mask,mode,reg); r=(D[dr]&s)&mask; SETNZ(r); D[dr]=(D[dr]&~mask)|r; }
        else { u32 a=EAADDR(mode,reg,sz); cyc+=MEMC(sz); s=RDV(sz,a); r=(D[dr]&s)&mask; SETNZ(r); cyc+=MEMC(sz); WRV(sz,a,r); } }
      break;
    }

    /* ================= line E: shifts and rotates ================= */
    case 14: {
      if(((op>>6)&3u)==3u){ /* memory, word, by 1 */
        u32 type=(op>>9)&3u,left=(op>>8)&1u,mode=(op>>3)&7u,reg=op&7u;
        if(!SHIFT_FAST_OK(type,left)||mode<2u||!DST_OK(mode,reg)) goto slow;
        u32 a=EAADDR(mode,reg,2); cyc+=4u; u32 v=RD16(a);
        u64 sh=do_shift(v,1u,0xFFFFu,type,left,sr); sr=(u32)(sh>>32); cyc+=2u;
        cyc+=4u; WR16(a,(u32)sh); break;
      }
      u32 szc=(op>>6)&3u,left=(op>>8)&1u,type=(op>>3)&3u,reg=op&7u,cnt=(op>>9)&7u;
      if(!SHIFT_FAST_OK(type,left)) goto slow;
      if(op&0x20u) cnt=D[cnt]&63u; else if(!cnt) cnt=8u;
      { u32 mask=szm[szc]; u64 sh=do_shift(D[reg],cnt,mask,type,left,sr); sr=(u32)(sh>>32); cyc+=2u*cnt;
        D[reg]=(D[reg]&~mask)|((u32)sh&mask); }
      break;
    }

    default:
    slow: {
      /* A/F lines, privileged, BCD, MOVEP, DIV, SR forms, TRAP, ... */
      u32 old_sr=sr;
      C.pc=pc-2u; C.cycles=cyc-4u; C.sr=(u16)sr;
      ref_step();
      pc=C.pc; cyc=C.cycles; sr=C.sr;
      if(C.stopped||((old_sr^sr)&0x2700u)) limit=cyc;
      AOT_LEAVE(pc);
      break;
    }
    }
    next_insn: ;
  }while((s32)(limit-cyc)>0);
  C.pc=pc; C.cycles=cyc; C.sr=(u16)sr;
}
