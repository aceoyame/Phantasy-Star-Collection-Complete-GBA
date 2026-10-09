#ifndef PSC8_FAST_OPLL_H
#define PSC8_FAST_OPLL_H
#include <stdint.h>
#include <stddef.h>
/* Per-channel decoded patch/frequency data used by the block renderer; recomputed only when a register it depends on is written
 * (fastopll_data marks cdirty). Derived state: not part of the synthesis state (fastopll_calc never reads it). */
typedef struct {
  uint32_t inc_m, inc_c;                 /* phase increments per sample */
  uint16_t msus, csus;                   /* sustain levels */
  uint8_t tl, fb, volf, halfm, halfc, nz;
  uint8_t mar_i, mdr_i, mrr_i, car_i, cdr_i, crr_i;   /* rate indices (release falls back to 5 when 0) */
} FmChP;
typedef struct {
  uint8_t reg[64],adr,stage[9],mod_stage[9],key_prev[9];
  uint16_t env[9],mod_env[9];
  int16_t prevm[9];
  uint32_t pm[9],pc[9],lfsr;
  uint16_t renv[5]; uint8_t rkey,rcnt;   /* rhythm: per-drum envelope (BD,SD,TOM,CYM,HH order of reg 0x0E bits 4..0), previous key bits */
  uint16_t cdirty; FmChP chp[9];         /* decode cache (keep last: tests compare the state before it) */
} FastOPLL;
#define FASTOPLL_STATE_BYTES offsetof(FastOPLL,cdirty)
void fastopll_reset(FastOPLL *o);
void fastopll_addr(FastOPLL *o,uint8_t a);
void fastopll_data(FastOPLL *o,uint8_t v);
int fastopll_calc(FastOPLL *o);
int fastopll_active(const FastOPLL *o);
void fastopll_rhythm(FastOPLL *o, int16_t *acc, unsigned n);
void fastopll_render(FastOPLL *o, int16_t *acc, unsigned n);
#endif
