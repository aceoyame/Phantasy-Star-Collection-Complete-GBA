#include <stdint.h>
#include <stddef.h>
#include "z80a.h"
#include "z80a_glue.h"
#define RES ((volatile uint32_t*)0x0203E000)
#define TXT ((volatile char*)0x02034000)
static uint8_t memA[65536] __attribute__((aligned(4)));
static uint8_t memB[65536] __attribute__((aligned(4)));
static uint8_t base[65536];
static uint32_t logA[16], logB[16]; static unsigned nA, nB, txtn, done, mode;
void *memset(void *p,int c,size_t n){uint8_t *d=p;while(n--)*d++=c;return p;}
void *memcpy(void *p,const void *s,size_t n){uint8_t *d=p;const uint8_t *q=s;while(n--)*d++=*q++;return p;}
void __aeabi_memcpy(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memcpy4(void *p,const void*s,size_t n){memcpy(p,s,n);}
void __aeabi_memclr(void *p,size_t n){memset(p,0,n);}
void __aeabi_memclr4(void *p,size_t n){memset(p,0,n);}
void z80a_wr_slow(uint32_t a, uint32_t v){ a &= 0xffff; memA[a] = (uint8_t)v; if (nA < 16) logA[nA++] = (a << 8) | (v & 255); }
static void putc_(char c){ if (txtn < 60000) TXT[txtn++] = c; }
uint32_t z80a_in_c(uint32_t port){
  (void)port;
  if (mode == 1) {
    unsigned c = Z80A_U32(Z80A_BC) & 255, de = Z80A_U32(Z80A_DE);
    if (c == 2) putc_((char)(de & 255));
    else if (c == 9) { unsigned a = de; while (memA[a & 0xffff] != '$') putc_(memA[a++ & 0xffff]); }
    return 0xff;
  }
  return 0x5a;
}
void z80a_out_c(uint32_t port, uint32_t v){ (void)port; (void)v; done = 1; }
/* reference C core */
static uint32_t cin(void){ return 0x5a; }
#define RD8(a) memB[(a)]
#define WR8(a,v) do{ uint32_t _a=(a)&0xffff; memB[_a]=(uint8_t)(v); if(nB<16) logB[nB++]=(_a<<8)|((v)&255);}while(0)
#define PORT_IN(p) cin()
#define PORT_OUT(p,v) ((void)0)
#define Z80F_STATIC static
#include "z80f.h"
static uint32_t rng = 12345;
static uint32_t rnd(void){ rng = rng * 1103515245u + 12345u; return rng >> 16; }
static void maps(void){ for (unsigned i = 0; i < 64; i++) ((const uint8_t**)z80a_ctx)[i] = memA; for (unsigned i = 64; i < 68; i++) ((const uint8_t**)z80a_ctx)[i] = memA - 0x10000; }
extern const uint8_t zexdoc_bin[], zexdoc_end[];
void test_main(void){
  z80a_setup(); maps(); z80f_tables();
  mode = *(volatile uint32_t*)0x0203FF00;
  if (mode == 1) {           /* ZEXDOC on the assembly core */
    z80a_reset(); maps();
    memset(memA, 0, 65536); memcpy(memA + 0x100, zexdoc_bin, (size_t)(zexdoc_end - zexdoc_bin));
    memA[0] = 0xD3; memA[1] = 0x00; memA[5] = 0xDB; memA[6] = 0x00; memA[7] = 0xC9;
    Z80A_U32(Z80A_PC) = 0x100;
    uint32_t total = 0;
    while (!done) { int32_t r = z80a_run(1000000); total += 1000000 - r; RES[1] = total; RES[2] = txtn; }
    RES[0] = 0x5A455844u; RES[3] = 1;
    for (;;) {}
  }
  /* mode 0: every opcode form, random state, one instruction, against the C core */
  for (unsigned i = 0; i < 65536; i++) base[i] = (uint8_t)rnd();
  memcpy(memA, base, 65536); memcpy(memB, base, 65536);
  unsigned bad = 0, tests = 0;
  static const int pre[6][2] = {{-1,-1},{0xCB,-1},{0xED,-1},{0xDD,-1},{0xFD,-1},{0xDD,0xCB}};
  for (unsigned p = 0; p < 7; p++) for (unsigned o = 0; o < 256; o++) for (unsigned s = 0; s < 6; s++) {
    uint8_t c[8]; unsigned n = 0;
    unsigned pp = p == 6 ? 5 : p;
    if (pre[pp][0] >= 0) c[n++] = (uint8_t)(p == 6 ? 0xFD : pre[pp][0]);
    if (pre[pp][1] >= 0) { c[n++] = (uint8_t)pre[pp][1]; c[n++] = (uint8_t)rnd(); }
    c[n++] = (uint8_t)o; c[n++] = (uint8_t)rnd(); c[n++] = (uint8_t)rnd(); c[n++] = (uint8_t)rnd();
    unsigned pc = 0x8000 + (rnd() & 0x3ff);
    for (unsigned k = 0; k < n; k++) memA[pc + k] = memB[pc + k] = c[k];
    Z80F z; z80f_reset(&z);
    z.pc = pc; z.sp = rnd(); z.a = rnd(); z.f = rnd(); z.b = rnd(); z.c = rnd(); z.d = rnd(); z.e = rnd(); z.h = rnd(); z.l = rnd();
    z.ix = rnd(); z.iy = rnd(); z.a_ = rnd(); z.f_ = rnd(); z.b_ = rnd(); z.c_ = rnd(); z.d_ = rnd(); z.e_ = rnd(); z.h_ = rnd(); z.l_ = rnd();
    z.iff1 = z.iff2 = 1; z.im = 1; z.i = rnd();
    if (s & 1) { z.b = 0; z.c = 1; }                       /* exercise block-op termination */
    z80a_reset(); maps();
    Z80A_U32(Z80A_PC) = z.pc; Z80A_U32(Z80A_SP) = z.sp; Z80A_U32(Z80A_A) = z.a; Z80A_U32(Z80A_F) = z.f;
    Z80A_U32(Z80A_BC) = (z.b << 8) | z.c; Z80A_U32(Z80A_DE) = (z.d << 8) | z.e; Z80A_U32(Z80A_HL) = (z.h << 8) | z.l;
    Z80A_U32(Z80A_IX) = z.ix; Z80A_U32(Z80A_IY) = z.iy; Z80A_U32(Z80A_AF2) = (z.a_ << 8) | z.f_;
    Z80A_U32(Z80A_BC2) = (z.b_ << 8) | z.c_; Z80A_U32(Z80A_DE2) = (z.d_ << 8) | z.e_; Z80A_U32(Z80A_HL2) = (z.h_ << 8) | z.l_;
    Z80A_U8(Z80A_IFF1) = 1; Z80A_U8(Z80A_IFF2) = 1; Z80A_U8(Z80A_IM) = 1; Z80A_U8(Z80A_I) = z.i;
    nA = nB = 0;
    int32_t rem = z80a_run(1); uint32_t ca = (uint32_t)(1 - rem);
    z.cyc = 0; z80f_run(&z, 1); uint32_t cb = z.cyc;
    unsigned mism = 0;
    uint32_t apc = Z80A_U32(Z80A_PC) & 0xffff;
    if (apc != z.pc) mism |= 1;
    if ((Z80A_U32(Z80A_A) & 255) != z.a) mism |= 2;
    uint32_t af = Z80A_U32(Z80A_F) & 255;
    unsigned edrand = (pp == 2 && ((o & 0xC7) == 0x57));   /* LD A,R / LD A,I: R differs by design */
    if (!edrand && (af & 0xff) != z.f) mism |= 4;
    if (Z80A_U32(Z80A_BC) != (uint32_t)((z.b << 8) | z.c)) mism |= 8;
    if (Z80A_U32(Z80A_DE) != (uint32_t)((z.d << 8) | z.e)) mism |= 16;
    if (Z80A_U32(Z80A_HL) != (uint32_t)((z.h << 8) | z.l)) mism |= 32;
    if (Z80A_U32(Z80A_SP) != z.sp) mism |= 64;
    if (Z80A_U32(Z80A_IX) != z.ix || Z80A_U32(Z80A_IY) != z.iy) mism |= 128;
    if (ca != cb) mism |= 256;
    if (nA != nB) mism |= 512; else for (unsigned k = 0; k < nA; k++) if (logA[k] != logB[k]) mism |= 1024;
    if (Z80A_U8(Z80A_IFF1) != z.iff1 || Z80A_U8(Z80A_IM) != z.im) mism |= 2048;
    if (Z80A_U32(Z80A_AF2) != (uint32_t)((z.a_ << 8) | z.f_) || Z80A_U32(Z80A_BC2) != (uint32_t)((z.b_ << 8) | z.c_) || Z80A_U32(Z80A_HL2) != (uint32_t)((z.h_ << 8) | z.l_)) mism |= 4096;
    if (Z80A_U8(Z80A_HALTED) != z.halted) mism |= 8192;
    if (edrand) mism &= ~2u;
    tests++;
    if (mism && bad < 200) { volatile uint32_t *r = RES + 16 + bad * 8; r[0] = (p << 16) | (o << 8) | s; r[1] = mism; r[2] = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
      r[3] = (apc << 16) | z.pc; r[4] = (af << 8) | z.f; r[5] = (ca << 16) | cb; r[6] = (Z80A_U32(Z80A_HL) << 16) | ((z.h << 8) | z.l); r[7] = (Z80A_U32(Z80A_BC) << 16) | ((z.b << 8) | z.c); }
    if (mism) bad++;
    for (unsigned k = 0; k < nA; k++) memA[logA[k] >> 8] = base[logA[k] >> 8];
    for (unsigned k = 0; k < nB; k++) memB[logB[k] >> 8] = base[logB[k] >> 8];
    for (unsigned k = 0; k < n; k++) memA[pc + k] = memB[pc + k] = base[pc + k];
  }
  RES[1] = tests; RES[2] = bad; RES[0] = 0x4F505354u;
  for (;;) {}
}
