/* PSC8 fast Z80 core (CP08).
 *
 * Written for the ARM7TDMI: one large interpreter function per run slice with the
 * register file in locals, eagerly computed F (table driven), memory reads through a
 * 1 KiB page-pointer table supplied by the includer, and interrupt checks only where
 * they can change (slice entry, EI, RETI/RETN) instead of after every instruction.
 * Instruction timings are the documented Z80 T-states (same tables as the previous core).
 *
 * The includer defines, before including this file:
 *   RD8(a)        read byte at 16-bit address a (a < 0x10000)
 *   WR8(a,v)      write byte
 *   PORT_IN(p)    / PORT_OUT(p,v)   I/O; the core stores cyc in z->cyc first (SYNC_OUT)
 *   Z80F_STATIC   storage class for z80f_run (e.g. "static" or a section attribute)
 * Validated on the host with ZEXDOC (see tests/zex.c).
 */
#ifndef PSC8_Z80F_H
#define PSC8_Z80F_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint32_t cyc;                              /* T-state counter (wraps; compare with signed difference) */
  uint16_t pc, sp, ix, iy;
  uint8_t a, f, b, c, d, e, h, l;
  uint8_t a_, f_, b_, c_, d_, e_, h_, l_;
  uint8_t i, r, rhi, im, iff1, iff2, halted, int_pending, nmi_pending, int_data, irqchk;
} Z80F;

#ifndef Z80F_DATA
#define Z80F_DATA
#endif
#ifndef Z80F_COLD
#define Z80F_COLD Z80F_STATIC
#endif
#ifndef Z80F_CDATA
#define Z80F_CDATA
#endif
enum { CF = 1, NF = 2, PF = 4, XF = 8, HF = 16, YF = 32, ZF = 64, SF = 128 };

static uint8_t SZ[256] Z80F_DATA, SZP[256] Z80F_DATA, SZHV_INC[256] Z80F_DATA, SZHV_DEC[256] Z80F_DATA;

static const uint8_t z80f_cyc_00[256] Z80F_CDATA = {4,10,7,6,4,4,7,4,4,11,7,6,4,4,7,4,8,10,7,6,4,4,7,4,12,11,7,6,4,4,7,4,7,10,16,6,4,4,7,4,7,11,16,6,4,4,7,4,7,10,13,6,11,11,10,4,7,11,13,6,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,7,7,7,7,7,7,4,7,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,5,10,10,10,10,11,7,11,5,10,10,0,10,17,7,11,5,10,10,11,10,11,7,11,5,4,10,11,10,0,7,11,5,10,10,19,10,11,7,11,5,4,10,4,10,0,7,11,5,10,10,4,10,11,7,11,5,6,10,4,10,0,7,11};
static const uint8_t z80f_cyc_ed[256] Z80F_CDATA = {8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,12,12,15,20,8,14,8,9,12,12,15,20,8,14,8,9,12,12,15,20,8,14,8,9,12,12,15,20,8,14,8,9,12,12,15,20,8,14,8,18,12,12,15,20,8,14,8,18,12,12,15,20,8,14,8,8,12,12,15,20,8,14,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,16,16,16,16,8,8,8,8,16,16,16,16,8,8,8,8,16,16,16,16,8,8,8,8,16,16,16,16,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8};
static const uint8_t z80f_cyc_ddfd[256] Z80F_CDATA = {4,4,4,4,4,4,4,4,4,15,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,15,4,4,4,4,4,4,4,14,20,10,8,8,11,4,4,15,20,10,8,8,11,4,4,4,4,4,23,23,19,4,4,15,4,4,4,4,4,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,8,8,8,8,8,8,19,8,8,8,8,8,8,8,19,8,19,19,19,19,19,19,4,19,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,8,8,19,4,4,4,4,4,4,4,4,4,4,4,4,0,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,14,4,23,4,15,4,4,4,8,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,10,4,4,4,4,4,4};

static void z80f_tables(void) {
  for (unsigned v = 0; v < 256; v++) {
    unsigned p = v; p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
    uint8_t sz = (uint8_t)((v & (SF | YF | XF)) | (v ? 0 : ZF));
    SZ[v] = sz; SZP[v] = (uint8_t)(sz | ((p & 1) ? 0 : PF));
    SZHV_INC[v] = (uint8_t)(sz | (v == 0x80 ? PF : 0) | ((v & 15) == 0 ? HF : 0));
    SZHV_DEC[v] = (uint8_t)(sz | NF | (v == 0x7f ? PF : 0) | ((v & 15) == 15 ? HF : 0));
  }
}

static void z80f_reset(Z80F *z) {
  uint8_t *p = (uint8_t *)z; for (unsigned i = 0; i < sizeof(*z); i++) p[i] = 0;
  z->sp = 0xffff; z->a = 0xff; z->f = 0xff;
  z80f_tables();
}

#define FETCH() (_fb = RD8(PC), PC = (PC + 1) & 0xffff, _fb)
#define FETCH16() (_fw = RD8(PC) | (RD8((PC + 1) & 0xffff) << 8), PC = (PC + 2) & 0xffff, _fw)
#define PUSH16(v) do { uint32_t _v = (v); SP = (SP - 2) & 0xffff; WR8((SP + 1) & 0xffff, _v >> 8); WR8(SP, _v & 255); } while (0)
#define POP16() (_fw = RD8(SP) | (RD8((SP + 1) & 0xffff) << 8), SP = (SP + 2) & 0xffff, _fw)
#define EA_IX ((IX + (int8_t)FETCH()) & 0xffff)
#define EA_IY ((IY + (int8_t)FETCH()) & 0xffff)
#define EA_XY ((XY + (int8_t)FETCH()) & 0xffff)
#define SYNC_OUT() do { z->cyc = cyc; z->a = A; z->b = B; z->c = C; z->d = D; z->e = E; z->h = H; z->l = L; z->pc = PC; } while (0)
#define SYNC_IN() do { } while (0)

#define OP_ADD(v, cy) do { uint32_t _v = (v), _r = A + _v + (cy); \
  F = SZ[_r & 255] | ((_r >> 8) & CF) | ((A ^ _v ^ _r) & HF) | (((_v ^ A ^ 0x80) & (_v ^ _r) & 0x80) >> 5); A = _r & 255; } while (0)
#define OP_SUB(v, cy) do { uint32_t _v = (v), _r = A - _v - (cy); \
  F = SZ[_r & 255] | ((_r >> 8) & CF) | NF | ((A ^ _v ^ _r) & HF) | (((_v ^ A) & (A ^ _r) & 0x80) >> 5); A = _r & 255; } while (0)
#define OP_CP(v) do { uint32_t _v = (v), _r = A - _v; \
  F = (SZ[_r & 255] & ~(YF | XF)) | (_v & (YF | XF)) | ((_r >> 8) & CF) | NF | ((A ^ _v ^ _r) & HF) | (((_v ^ A) & (A ^ _r) & 0x80) >> 5); } while (0)
#define OP_ADD16(dst, v) do { uint32_t _a = (dst), _v = (v), _r = _a + _v; \
  F = (F & (SF | ZF | PF)) | ((_r >> 16) & CF) | (((_a ^ _v ^ _r) >> 8) & HF) | ((_r >> 8) & (YF | XF)); (dst) = _r & 0xffff; } while (0)
#define OP_ADC16(v) do { uint32_t _a = (H << 8) | L, _v = (v), _r = _a + _v + (F & CF); \
  F = ((_r >> 16) & CF) | (((_a ^ _v ^ _r) >> 8) & HF) | (((_v ^ _a ^ 0x8000) & (_v ^ _r) & 0x8000) >> 13) | ((_r >> 8) & (SF | YF | XF)) | ((_r & 0xffff) ? 0 : ZF); \
  H = (_r >> 8) & 255; L = _r & 255; } while (0)
#define OP_SBC16(v) do { uint32_t _a = (H << 8) | L, _v = (v), _r = _a - _v - (F & CF); \
  F = NF | ((_r >> 16) & CF) | (((_a ^ _v ^ _r) >> 8) & HF) | (((_v ^ _a) & (_a ^ _r) & 0x8000) >> 13) | ((_r >> 8) & (SF | YF | XF)) | ((_r & 0xffff) ? 0 : ZF); \
  H = (_r >> 8) & 255; L = _r & 255; } while (0)
#define OP_RLC(v) do { v = ((v << 1) | (v >> 7)) & 255; F = SZP[v] | (v & CF); } while (0)
#define OP_RRC(v) do { uint32_t _c = v & 1; v = ((v >> 1) | (v << 7)) & 255; F = SZP[v] | _c; } while (0)
#define OP_RL(v)  do { uint32_t _c = v >> 7; v = ((v << 1) | (F & CF)) & 255; F = SZP[v] | _c; } while (0)
#define OP_RR(v)  do { uint32_t _c = v & 1; v = (v >> 1) | ((F & CF) << 7); F = SZP[v] | _c; } while (0)
#define OP_SLA(v) do { uint32_t _c = v >> 7; v = (v << 1) & 255; F = SZP[v] | _c; } while (0)
#define OP_SRA(v) do { uint32_t _c = v & 1; v = (v >> 1) | (v & 0x80); F = SZP[v] | _c; } while (0)
#define OP_SLL(v) do { uint32_t _c = v >> 7; v = ((v << 1) | 1) & 255; F = SZP[v] | _c; } while (0)
#define OP_SRL(v) do { uint32_t _c = v & 1; v >>= 1; F = SZP[v] | _c; } while (0)
#define OP_BIT(v, n) do { uint32_t _m = (v) & (1u << (n)); \
  F = (F & CF) | HF | ((v) & (YF | XF)) | (_m ? (_m & SF) : (ZF | PF)); } while (0)
/* BIT n,(HL): undocumented X/Y come from the internal WZ register, which this core does not track. */
#define OP_BITM(v, n) do { uint32_t _m = (v) & (1u << (n)); F = (F & CF) | HF | (_m ? (_m & SF) : (ZF | PF)); } while (0)
#define OP_BITX(v, n, a) do { uint32_t _m = (v) & (1u << (n)); \
  F = (F & CF) | HF | (((a) >> 8) & (YF | XF)) | (_m ? (_m & SF) : (ZF | PF)); } while (0)
#define OP_LDI(dir) do { uint32_t _hl = (H << 8) | L, _de = (D << 8) | E, _bc = (B << 8) | C; uint32_t _v = RD8(_hl); WR8(_de, _v); \
  _hl = (_hl + (dir)) & 0xffff; _de = (_de + (dir)) & 0xffff; _bc = (_bc - 1) & 0xffff; \
  H = _hl >> 8; L = _hl & 255; D = _de >> 8; E = _de & 255; B = _bc >> 8; C = _bc & 255; \
  _v += A; F = (F & (SF | ZF | CF)) | (_bc ? PF : 0) | (_v & XF) | ((_v << 4) & YF); } while (0)
#define OP_CPI(dir) do { uint32_t _hl = (H << 8) | L, _bc = (B << 8) | C; uint32_t _v = RD8(_hl), _r = (A - _v) & 255; \
  _hl = (_hl + (dir)) & 0xffff; _bc = (_bc - 1) & 0xffff; H = _hl >> 8; L = _hl & 255; B = _bc >> 8; C = _bc & 255; \
  F = (F & CF) | NF | (SZ[_r] & ~(YF | XF)) | ((A ^ _v ^ _r) & HF) | (_bc ? PF : 0); \
  _r = (_r - ((F & HF) ? 1 : 0)) & 255; F |= (_r & XF) | ((_r << 4) & YF); } while (0)
/* INI/OUTI family: Z and N as in the previous PSC8 core (the other flags are left unchanged). */
#define OP_INI(dir) do { uint32_t _hl = (H << 8) | L; SYNC_OUT(); uint32_t _v = PORT_IN(C); WR8(_hl, _v); \
  _hl = (_hl + (dir)) & 0xffff; H = _hl >> 8; L = _hl & 255; B = (B - 1) & 255; \
  F = (F & ~ZF) | NF | (B ? 0 : ZF); } while (0)
#define OP_OUTI(dir) do { uint32_t _hl = (H << 8) | L; uint32_t _v = RD8(_hl); SYNC_OUT(); PORT_OUT(C, _v); \
  _hl = (_hl + (dir)) & 0xffff; H = _hl >> 8; L = _hl & 255; B = (B - 1) & 255; \
  F = (F & ~ZF) | NF | (B ? 0 : ZF); } while (0)
#define OP_DAA() do { uint32_t _c = 0, _a = A, _f = F; \
  if ((_f & HF) || (_a & 15) > 9) _c = 6; \
  if ((_f & CF) || _a > 0x99) { _c |= 0x60; } \
  uint32_t _r = (_f & NF) ? (_a - _c) & 255 : (_a + _c) & 255; \
  F = SZP[_r] | (_f & NF) | ((_f & CF) || _a > 0x99 ? CF : 0) | ((_a ^ _r) & HF); A = _r; } while (0)
#define HALT_OP() do { z->halted = 1; PC = (PC - 1) & 0xffff; \
  { int32_t _left = (int32_t)(target - cyc); if (_left > 0) cyc += (uint32_t)(_left + 3) & ~3u; } } while (0)

#include "z80f_ops.inc"


#define LOAD_REGS() uint32_t A = z->a, F = z->f, B = z->b, C = z->c, D = z->d, E = z->e, H = z->h, L = z->l; \
  uint32_t PC = z->pc, SP = z->sp, cyc = z->cyc; uint32_t _fb, _fw; (void)_fb; (void)_fw;
#define STORE_REGS() do { z->a = A; z->f = F; z->b = B; z->c = C; z->d = D; z->e = E; z->h = H; z->l = L; \
  z->pc = PC; z->sp = SP; z->cyc = cyc; } while (0)

static const uint8_t z80f_roff[8] = {
  offsetof(Z80F, b), offsetof(Z80F, c), offsetof(Z80F, d), offsetof(Z80F, e),
  offsetof(Z80F, h), offsetof(Z80F, l), 0, offsetof(Z80F, a) };

/* CB xx: generic decode on the register file in *z (compact; registers are flushed by the caller). */
__attribute__((noinline)) Z80F_STATIC void z80f_cb(Z80F *z, uint32_t op) {
  uint32_t r = op & 7, a = 0, v, F = z->f;
  uint8_t *rp = (uint8_t *)z + z80f_roff[r];
  if (r == 6) { a = ((uint32_t)z->h << 8) | z->l; v = RD8(a); } else v = *rp;
  uint32_t n = (op >> 3) & 7;
  switch (op >> 6) {
    case 0:
      switch (n) { case 0: OP_RLC(v); break; case 1: OP_RRC(v); break; case 2: OP_RL(v); break; case 3: OP_RR(v); break;
                   case 4: OP_SLA(v); break; case 5: OP_SRA(v); break; case 6: OP_SLL(v); break; default: OP_SRL(v); break; }
      break;
    case 1: if (r == 6) OP_BITM(v, n); else OP_BIT(v, n); z->f = F; return;
    case 2: v &= ~(1u << n); break;
    default: v |= 1u << n; break;
  }
  if (r == 6) WR8(a, v); else *rp = (uint8_t)v;
  z->f = F;
}

/* DD/FD CB d xx at address a; the undocumented register copy is kept. */
__attribute__((noinline)) Z80F_STATIC void z80f_xcb(Z80F *z, uint32_t a, uint32_t op) {
  uint32_t r = op & 7, v = RD8(a), F = z->f, n = (op >> 3) & 7;
  switch (op >> 6) {
    case 0:
      switch (n) { case 0: OP_RLC(v); break; case 1: OP_RRC(v); break; case 2: OP_RL(v); break; case 3: OP_RR(v); break;
                   case 4: OP_SLA(v); break; case 5: OP_SRA(v); break; case 6: OP_SLL(v); break; default: OP_SRL(v); break; }
      break;
    case 1: OP_BITX(v, n, a); z->f = F; return;
    case 2: v &= ~(1u << n); break;
    default: v |= 1u << n; break;
  }
  WR8(a, v);
  if (r != 6) *((uint8_t *)z + z80f_roff[r]) = (uint8_t)v;
  z->f = F;
}

/* ED xx (registers flushed by the caller). Block moves/outputs loop here until done or the slice ends. */
__attribute__((noinline)) Z80F_COLD void z80f_ed(Z80F *z, uint32_t op, uint32_t target) {
  LOAD_REGS();
  uint32_t R = z->r;
  cyc += z80f_cyc_ed[op];
  switch (op) { Z80F_ED_CASES default: break; }
  z->r = R;
  STORE_REGS();
}

Z80F_STATIC void z80f_run(Z80F *z, uint32_t target) {
  uint32_t A = z->a, F = z->f, B = z->b, C = z->c, D = z->d, E = z->e, H = z->h, L = z->l;
  uint32_t PC = z->pc, SP = z->sp, IX = z->ix, IY = z->iy, R = z->r, cyc = z->cyc;
  uint32_t _fb, _fw, op, irq_check;
  (void)_fb; (void)_fw;
  if (z->halted && !z->int_pending && !z->nmi_pending) { int32_t left = (int32_t)(target - cyc); if (left > 0) z->cyc = cyc + ((uint32_t)(left + 3) & ~3u); return; }
  /* Like the previous PSC8 core, a pending interrupt is accepted after the first instruction of the slice
   * (or after one HALT NOP cycle), keeping guest timing identical. */
  if (z->halted) { cyc += 4; irq_check = 1; } else irq_check = 2;
  while ((int32_t)(cyc - target) < 0) {
    if (irq_check) {
      if (--irq_check == 0) {
        if (z->nmi_pending) {
          z->nmi_pending = 0; if (z->halted) { z->halted = 0; PC = (PC + 1) & 0xffff; }
          z->iff1 = 0; PUSH16(PC); PC = 0x66; cyc += 11; R++;
        } else if (z->int_pending && z->iff1) {
          if (z->halted) { z->halted = 0; PC = (PC + 1) & 0xffff; }
          z->iff1 = z->iff2 = 0; z->int_pending = 0; R++;   /* accepted: line request consumed (as the previous PSC8 core) */
          if (z->im == 2) { uint32_t v = ((uint32_t)z->i << 8) | z->int_data; PUSH16(PC); PC = RD8(v) | (RD8((v + 1) & 0xffff) << 8); cyc += 19; }
          else { PUSH16(PC); PC = 0x38; cyc += z->im == 1 ? 13 : 11; }
        }
        if (z->halted) { int32_t left = (int32_t)(target - cyc); if (left > 0) cyc += (uint32_t)(left + 3) & ~3u; break; }
        if ((int32_t)(cyc - target) >= 0) break;
      }
    }
    op = FETCH(); R++;
    cyc += z80f_cyc_00[op];
  dispatch:
    switch (op) {
      Z80F_MAIN_CASES
      case 0xCB: {
        op = FETCH(); R++;
        cyc += ((op & 7) == 6) ? (((op & 0xc0) == 0x40) ? 12 : 15) : 8;
        z->a = A; z->f = F; z->b = B; z->c = C; z->d = D; z->e = E; z->h = H; z->l = L;
        z80f_cb(z, op);
        A = z->a; F = z->f; B = z->b; C = z->c; D = z->d; E = z->e; H = z->h; L = z->l;
      } break;
      case 0xED: {
        op = FETCH(); R++;
        /* hot block transfers stay in the fast loop; everything else goes to the (cold) ED handler */
        if (op == 0xB3) { cyc += 16; for(;;){OP_OUTI(1);if(!B)break;cyc+=5;if((int32_t)(cyc-target)>=0){PC=(PC-2)&0xffff;break;}cyc+=16;} break; }
        if (op == 0xA3) { cyc += 16; OP_OUTI(1); break; }
        if (op == 0xB0) { cyc += 16; for(;;){OP_LDI(1);if(!(B|C))break;cyc+=5;if((int32_t)(cyc-target)>=0){PC=(PC-2)&0xffff;break;}cyc+=16;} break; }
        if (op == 0xA0) { cyc += 16; OP_LDI(1); break; }
        z->r = R; STORE_REGS();
        z80f_ed(z, op, target);
        A = z->a; F = z->f; B = z->b; C = z->c; D = z->d; E = z->e; H = z->h; L = z->l;
        PC = z->pc; SP = z->sp; cyc = z->cyc; R = z->r;
        if (z->irqchk) { z->irqchk = 0; irq_check = 1; }
      } break;
      case 0xDD: case 0xFD: {
        uint32_t useiy = (op == 0xFD), XY;
        for (;;) {
          op = FETCH(); R++;
          if (op == 0xDD) { useiy = 0; cyc += 4; continue; }
          if (op == 0xFD) { useiy = 1; cyc += 4; continue; }
          break;
        }
        XY = useiy ? IY : IX;
        if (op == 0xCB) {
          uint32_t a = EA_XY; uint32_t x = FETCH();
          cyc += ((x & 0xc0) == 0x40) ? 20 : 23;
          z->a = A; z->f = F; z->b = B; z->c = C; z->d = D; z->e = E; z->h = H; z->l = L;
          z80f_xcb(z, a, x);
          A = z->a; F = z->f; B = z->b; C = z->c; D = z->d; E = z->e; H = z->h; L = z->l;
          break;
        }
        { uint32_t t = z80f_cyc_ddfd[op]; cyc += (t == 4) ? 4 + z80f_cyc_00[op] : t; }
        switch (op) {
          Z80F_XY_CASES
          default:
            if (op == 0xED) { PC = (PC - 1) & 0xffff; break; }   /* DD ED xx: prefix ignored */
            goto dispatch;                                        /* not an index instruction: plain opcode */
        }
        if (useiy) IY = XY; else IX = XY;
      } break;
    }
  }
  z->a = A; z->f = F; z->b = B; z->c = C; z->d = D; z->e = E; z->h = H; z->l = L;
  z->pc = PC; z->sp = SP; z->ix = IX; z->iy = IY; z->r = R; z->cyc = cyc;
}
#endif
