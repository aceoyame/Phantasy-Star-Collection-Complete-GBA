/* gsnd_eg.h: YM2612 envelope generator as the Genesis adventures' sound driver sees it (key-on/off, rates, key scale,
 * sustain level), event driven: an operator only costs time on the ticks where its state can change.
 *
 * The register decoding, rate tables and the per-tick state machine follow the OPN2 (die-extracted increment table in
 * gfm_tabs.h).  One tick = three YM2612 samples (the envelope clock divider), GSND_TICKS_PER_FRAME ticks per sound frame.
 * Only the carriers of a patch need an envelope here: the modulators' envelopes are part of the baked timbre snapshots. */
#pragma once
#include <stdint.h>

#define GSND_NEVER 0xFFFFFFFFu
enum { EG_ATK = 0, EG_DEC = 1, EG_SUS = 2, EG_REL = 3 };

typedef struct {
  uint32_t next_ev;       /* tick at which eg_clock() next has to run, GSND_NEVER if it cannot change */
  uint16_t env;           /* attenuation 0 (loud) .. 0x3ff */
  uint16_t tl8;           /* total level << 3 */
  uint16_t sl;            /* sustain level (env units) */
  uint8_t state, key;
  uint8_t rate[4];        /* effective 6-bit rates: attack, decay, sustain, release */
} eop;

static inline uint32_t eg_eff_rate(uint32_t raw, uint32_t ksr) {
  if (!raw) return 0;
  uint32_t r = raw + ksr;
  return r > 63 ? 63 : r;
}

/* one envelope tick (state transitions every call, increments only on the rate's own ticks) */
static inline void eg_clock(eop *o, uint32_t cnt) {
  if (o->state == EG_ATK && o->env == 0) o->state = EG_DEC;
  if (o->state == EG_DEC && o->env >= o->sl) o->state = EG_SUS;
  uint32_t rate = o->rate[o->state];
  uint32_t shift = rate >> 2;
  uint32_t c = cnt << shift;
  if (c & 0x7ff) return;
  uint32_t rb = (c >> (shift <= 11 ? 11 : shift)) & 7;
  uint32_t inc = (gfm_inc[rate] >> (4 * rb)) & 15;
  if (o->state == EG_ATK) {
    if (rate < 62) o->env = (uint16_t)(o->env + (((uint32_t)~(uint32_t)o->env * inc) >> 4));
  } else {
    uint32_t e = o->env + inc;
    o->env = (uint16_t)(e >= 0x400 ? 0x3ff : e);
  }
}

/* first tick >= now at which eg_clock() can change anything (GSND_NEVER if it cannot) */
static inline uint32_t eg_next(const eop *o, uint32_t now) {
  uint32_t st = o->state;
  if (st == EG_ATK && o->env == 0) return now;
  if (st == EG_DEC && o->env >= o->sl) return now;
  if (st != EG_ATK && o->env >= 0x3ff) return GSND_NEVER;
  uint32_t rate = o->rate[st];
  if (rate < 2) return GSND_NEVER;
  if (st == EG_ATK && rate >= 62) return GSND_NEVER;
  uint32_t shift = rate >> 2;
  if (shift >= 11) return now;
  uint32_t period = 1u << (11 - shift);
  return (now + period - 1) & ~(period - 1);
}

/* CP12: prefix sums of the chip's eight-step increment pattern.  Stable
 * decay/sustain/release spans are additive, so their exact result can be
 * computed without revisiting every tick.  Attack and state transitions
 * keep the original tick path.  The generated constants live in IWRAM. */
#include "gsnd_eg_sum.h"

/* run every envelope tick up to and including `tend` */
static inline void eg_advance(eop *o, uint32_t tend) {
  while (o->next_ev <= tend) {
    uint32_t tk = o->next_ev;
    uint32_t st = o->state;
    if (st != EG_ATK && o->env < 0x3ff &&
        !(st == EG_DEC && o->env >= o->sl)) {
      uint32_t rate = o->rate[st], shift = rate >> 2;
      uint32_t ps = shift < 11 ? 11 - shift : 0;
      uint32_t n = ((tend - tk) >> ps) + 1;
      uint32_t b = (tk >> ps) & 7u, e = b + n;
      const uint8_t *p = gfm_inc_sum[rate];
      uint32_t inc = (e >> 3) * p[8] + p[e & 7u] - p[b];
      uint32_t env = o->env + inc;
      /* A decay crossing changes rate on the following tick; handle it
       * through the original path instead of moving that transition. */
      if (st != EG_DEC || env < o->sl) {
        o->env = (uint16_t)(env >= 0x400 ? 0x3ff : env);
        o->next_ev = o->env >= 0x3ff ? GSND_NEVER : tk + (n << ps);
        return;
      }
    }
    eg_clock(o, tk);
    o->next_ev = eg_next(o, tk + 1);
  }
}

static inline void eg_key_on(eop *o, uint32_t now) {
  if (o->state != EG_ATK) {
    o->state = EG_ATK;
    if (o->rate[EG_ATK] >= 62) o->env = 0;
  }
  o->key = 1;
  o->next_ev = eg_next(o, now + 1);
}
static inline void eg_key_off(eop *o, uint32_t now) {
  if (o->state < EG_REL) o->state = EG_REL;
  o->key = 0;
  o->next_ev = eg_next(o, now + 1);
}
