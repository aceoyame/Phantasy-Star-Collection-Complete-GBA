/* Shared setup for the assembly Z80 core (z80a.s). Requires z80a.h. */
#ifndef Z80A_GLUE_H
#define Z80A_GLUE_H
#include "z80a.h"
static uint16_t z80a_daa_table[2048];
static void z80a_setup(void){
  uint8_t *t = z80a_ctx + Z80A_TAB;            /* SZP +0, SZ +256, INC +512, DEC +768 */
  for (unsigned v = 0; v < 256; v++) {
    unsigned p = v; p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
    unsigned sz = (v & 0xA8) | (v ? 0 : 0x40);
    t[v] = (uint8_t)(sz | ((p & 1) ? 0 : 4));
    t[256 + v] = (uint8_t)sz;
    t[512 + v] = (uint8_t)(sz | (v == 0x80 ? 4 : 0) | ((v & 15) == 0 ? 0x10 : 0));
    t[768 + v] = (uint8_t)(sz | 2 | (v == 0x7f ? 4 : 0) | ((v & 15) == 15 ? 0x10 : 0));
  }
  for (unsigned i = 0; i < 2048; i++) {          /* index: A | C<<8 | N<<9 | H<<10 */
    unsigned a = i & 255, c = (i >> 8) & 1, n = (i >> 9) & 1, h = (i >> 10) & 1, cor = 0;
    if (h || (a & 15) > 9) cor = 6;
    if (c || a > 0x99) cor |= 0x60;
    unsigned r = n ? (a - cor) & 255 : (a + cor) & 255;
    unsigned pp = r; pp ^= pp >> 4; pp ^= pp >> 2; pp ^= pp >> 1;
    unsigned f = (r & 0xA8) | (r ? 0 : 0x40) | ((pp & 1) ? 0 : 4) | (n ? 2 : 0) | ((c || a > 0x99) ? 1 : 0) | ((a ^ r) & 0x10);
    z80a_daa_table[i] = (uint16_t)((r << 8) | f);
  }
  Z80A_U32(Z80A_DAATAB) = (uint32_t)z80a_daa_table;
  Z80A_U32(Z80A_MAINTAB) = (uint32_t)z80a_main_table;
}
static void z80a_reset(void){
  for (unsigned o = 0x120; o < 0x200; o++) z80a_ctx[o] = 0;
  Z80A_U32(Z80A_DAATAB) = (uint32_t)z80a_daa_table;
  Z80A_U32(Z80A_MAINTAB) = (uint32_t)z80a_main_table;
  Z80A_U32(Z80A_SP) = 0xffff; Z80A_U32(Z80A_A) = 0xff; Z80A_U32(Z80A_F) = 0xff;
}
#endif
