/* Genesis sound for the text adventures (CP09).  See mdsnd.h.
 *
 *   Z80 sound driver ---- YM2612 / PSG writes (time-stamped within the frame) ----> event queue
 *   event queue ---------> FM channels (patch captured at key-on -> baked snapshot tables, carrier envelope)
 *                          + SN76489 squares/noise ----> accumulate ----> int8 stereo
 *
 * The FM notes are not synthesised operator by operator: every patch/octave of the eight games was rendered offline
 * (additional/gsnd/bake) into a few timbre snapshots; a note plays the snapshot table at the right pitch, cross-fading
 * to the next snapshot as it ages, scaled by the real carrier envelope (exact YM2612 envelope generator, event driven)
 * and the driver's volume (carrier total level).  A patch that is not in the bank plays as plain carrier sines. */
#include "md.h"
#ifdef SND_DEBUG
#include <stdio.h>
#endif
#include "mdsnd.h"
#ifndef MD_HOST
#include "mdplat.h"
u32 snd_tz, snd_tz_max, snd_tr, snd_tr_max;   /* CPU cycles of the last / slowest frame: Z80 driver, synthesis+mix (read from the heartbeat) */
#define SND_TK(v) v
#else
#define SND_TK(v) 0
#endif
#include "gsnd/gfm_tabs.h"
#include "gsnd/gsnd_eg.h"
/* Experimental CP20: exact one-time EWRAM shadow of immutable FM tables. */
#ifndef MD_HOST
#define CP20_WAVE_BASE 0x0804D120u
#define CP20_WAVE_LEN  0x12540u
static s8 cp20_waves[CP20_WAVE_LEN] __attribute__((aligned(4)));
static void cp20_wave_init(void) {
  const u32 *src=(const u32 *)CP20_WAVE_BASE;
  u32 *dst=(u32 *)cp20_waves;
  for (u32 i=0;i<CP20_WAVE_LEN/4;i++) dst[i]=src[i];
}
#endif


/* ------------------------------------------------------------------ Z80 host */
static inline u32 zrd(u32 a);
static inline void zwr(u32 a, u32 v);
#define RD8(a) zrd(a)
#define WR8(a, v) zwr((a), (v))
#define PORT_IN(p) 0xFFu
#define PORT_OUT(p, v) ((void)0)
#define Z80F_STATIC static
#ifndef MD_HOST
#define Z80F_RUN_ATTR __attribute__((section(".text.rom"),target("thumb")))
#endif
#include "../psc8/core/z80f.h"

#define SLICE_T 192u                       /* fallback only; translated driver uses the frame boundary */
#define DRIVER_HASH 0xfdfaa135u                     /* FNV-1a of zram[0..4069]; the idle-loop skip is only used for this driver */
#define TS5 336                            /* one YM2612 sample = 67.2 Z80 T-states = 336/5 */

SndStats snd_stats;
int snd_nopsg;                             /* host tests: leave the PSG out of the mix */
#ifdef MD_HOST
int snd_noaot, snd_noskip, snd_slice;      /* host tests: interpreter only / no idle skip / slice length in T-states */
#define SLICE_LEN (snd_slice ? (u32)snd_slice : SLICE_T)
#else
#define SLICE_LEN SLICE_T
#endif
static Z80F z HOTDATA;
static u8 z_run, z_rst, z_busreq, z_skipok, aot_ok, aot_dead;
static u32 z_bank, z_fbase, z_fnext, z_hash_done, t_last, z_busreq_wr;
static u8 ym_addr[2], ym_r24, ym_r25, ym_r26, ym_r27, ym_status, tA_run, tB_run;
static s32 tA_left, tB_left;               /* in fifths of a T-state */
#ifdef MD_HOST
void (*snd_ev_hook)(u32 part, u32 reg, u32 val, u32 tstate);   /* host tests: every YM/PSG write (part 2 = PSG) */
#define EV_HOOK(p, r, v, te) do { if (snd_ev_hook) snd_ev_hook((p), (r), (v), (te) - z_fbase); } while (0)
#else
#define EV_HOOK(p, r, v, te) do {} while (0)
#endif

/* events: sample position within the frame + (kind << 16 | reg << 8 | val); kind 0/1 = YM part, 2 = PSG */
#define MAXEV 1536
static u16 ev_pos[MAXEV];
static u32 ev_d[MAXEV];
static u32 nev;
static inline void ev_push(u32 pos, u32 d) {
  if (nev < MAXEV) { ev_pos[nev] = (u16)pos; ev_d[nev++] = d; } else snd_stats.dropped++;
}
static inline u32 pos_of(u32 now) {
  u32 t = now - z_fbase;
  if (t >= SND_FRAME_T) t = SND_FRAME_T - 1;
  u32 p = (t * 62991u) >> 24;
  return p >= SND_N ? SND_N - 1 : p;
}

static inline u32 zrd(u32 a) {
  a &= 0xFFFFu;
  if (a < 0x4000u) return md_zram[a & 0x1FFFu];
  if (a < 0x6000u) return (a & 3u) == 0 ? ym_status : 0u;
  if (a < 0x8000u) return 0xFFu;
  u32 r = (z_bank << 15) | (a & 0x7FFFu);
  return r < m68k_romsize ? m68k_romp[r >> 12][r & 0xFFFu] : 0xFFu;
}

static void timers_step(u32 dt) {
  if (tA_run) {
    tA_left -= (s32)dt * 5;
    while (tA_left <= 0) { tA_left += (s32)(1024 - ((ym_r24 << 2) | (ym_r25 & 3))) * TS5; if (ym_r27 & 4) ym_status |= 1; }
  }
  if (tB_run) {
    tB_left -= (s32)dt * 5;
    while (tB_left <= 0) { tB_left += (s32)(256 - ym_r26) * 16 * TS5; if (ym_r27 & 8) ym_status |= 2; }
  }
}

/* The timers are advanced lazily, to the exact T-state of the access that looks at them (status read, register 0x27 write). */
static inline void timers_sync(u32 now) {
  s32 dt = (s32)(now - t_last);
  if (dt > 0) { t_last = now; if (tA_run | tB_run) timers_step((u32)dt); }
}

/* ts = start, te = end of the writing instruction (T-states, absolute) */
static __attribute__((noinline)) void ym_data(u32 part, u32 reg, u32 val, u32 ts, u32 te) {
  EV_HOOK(part, reg, val, te);
  if (part == 0 && reg >= 0x24 && reg <= 0x27) {
    timers_sync(ts);                                      /* an overflow that happened before this write reloads with the old period */
    if (reg == 0x24) ym_r24 = (u8)val; else if (reg == 0x25) ym_r25 = (u8)val; else if (reg == 0x26) ym_r26 = (u8)val;
    else {
      ym_r27 = (u8)val;
      if (val & 1) { tA_left = (s32)(1024 - ((ym_r24 << 2) | (ym_r25 & 3))) * TS5; tA_run = 1; } else tA_run = 0;
      if (val & 2) { tB_left = (s32)(256 - ym_r26) * 16 * TS5; tB_run = 1; } else tB_run = 0;
      if (val & 0x10) ym_status &= (u8)~1u;
      if (val & 0x20) ym_status &= (u8)~2u;
    }
    return;
  }
  if (reg == 0x28 || reg >= 0x30) ev_push(pos_of(te), (part << 16) | (reg << 8) | val);
}

static void zwr(u32 a, u32 v) {
  a &= 0xFFFFu;
  if (a < 0x4000u) {
#ifdef SND_DEBUG
    if ((a & 0x1FFFu) < 0xFE6u && md_zram[a & 0x1FFFu] != v) { static u8 seen[0x1000]; if (!seen[a & 0xFFF]) { seen[a & 0xFFF] = 1; fprintf(stderr, "CODEWRITE %04x=%02x frame %u pc %04x\n", a & 0x1FFF, v, snd_stats.z80_frames, z.pc); } }
#endif
    md_zram[a & 0x1FFFu] = (u8)v; return;
  }
  if (a < 0x6000u) { u32 p = (a >> 1) & 1u; if (!(a & 1u)) ym_addr[p] = (u8)v; else ym_data(p, ym_addr[p], v, z.cyc, z.cyc); return; }
  if (a < 0x6100u) { z_bank = (z_bank >> 1) | ((v & 1u) << 8); return; }
  if (a == 0x7F11u) { EV_HOOK(2, 0, v, z.cyc); ev_push(pos_of(z.cyc), (2u << 16) | v); return; }
}

/* ------------------------------------------------------------------ synthesis state */
#ifndef BLK
#define BLK 16u
#endif
static const u8 CARR[8] = {8, 8, 8, 8, 10, 14, 14, 15};
static const u8 NSV[8] = {1, 1, 1, 1, 2, 3, 3, 4};
static const u8 SVC[8][4] = {{3}, {3}, {3}, {3}, {1, 3}, {1, 2, 3}, {1, 2, 3}, {0, 1, 2, 3}};
static const u8 SVM[8][4] = {{15}, {15}, {15}, {15}, {3, 12}, {3, 5, 9}, {3, 4, 8}, {1, 2, 4, 8}};
static const u8 OP_OF[4] = {0, 2, 1, 3};                 /* register slot -> operator index */
static const u16 ST_T[8] = {80, 270, 670, 1470, 3210, 6960, 14700, 32100};   /* snapshot times in samples: 6, 20, 50, 110, 240, 520, 1100, 2400 ms */
static u32 st_recip[7] HOTDATA;
static u16 gain_tab[1024];                                /* once per voice/chunk; save IWRAM for executable sound paths */
static s8 sine8[256]; /* rare unbanked-patch fallback; leave IWRAM for rendering */

enum { SV_TABLE = 0, SV_SINE = 1, SV_NOISE = 2, SV_MUTE = 3 };   /* kinds of sub-voice */
typedef struct { const snd_zone *zone; u32 ph, step, nph, sps; u8 car, pure; } svoice;   /* sps: chip step the cached `step` was made from */
typedef struct {
  u16 bf; u8 alg, fb, pan, kon, dirty, active, nv, pad;
  u8 op[4][6];                                           /* raw registers in operator order: DT/MUL, TL, KS/AR, AM/DR, SR, SL/RR */
  eop e[4];
  u32 pstep[4];
  u32 age;                                               /* samples since key-on (frozen after key-off) */
  svoice v[4];
} fmch;
static fmch FM[6] HOTDATA;
/* Key-on snapshots immutable bank metadata into ordinary EWRAM. The wave
 * samples themselves keep the existing CP20 shadow and original format. */
/* CP56: decode immutable stage descriptors once at key-on; live
 * envelope, interpolation and phase values are still computed exactly. */
typedef struct { snd_zone zone; u32 tabs[8]; u16 gain5[8], noise5[8]; u8 shift[8]; } voice_meta;
static voice_meta voice_metadata[6][4];
static u8 latch[2];
static u32 g_tick, cur_tick;                             /* envelope tick at the start of the frame / at the event being applied */
static inline u32 tick_at(u32 p) { return g_tick + ((p * 5413u) >> 12); }   /* 296 envelope ticks per frame */

/* PSG */
static u16 psg_per[3], psg_lfsr;
static u8 psg_vol[4], psg_latch, psg_noise;
static u32 psg_ph[3], psg_step[3], psg_nph, psg_nstep;
static u32 psg_amp[16] HOTDATA;

/* the zone of this timbre played in octave `blk`: the zone of the same block, else the next higher block (a duller version of the
 * table, which cannot alias), else the nearest lower one */
static const snd_zone *zlookup(u32 h, u32 blk) {
  s32 lo = 0, hi = (s32)snd_nzones;
  while (lo < hi) {                                       /* first zone with hash >= h */
    s32 mid = (lo + hi) >> 1;
    if (snd_zones[mid].hash < h) lo = mid + 1; else hi = mid;
  }
  const snd_zone *best = 0; s32 bd = 99;
  for (s32 i = lo; i < (s32)snd_nzones && snd_zones[i].hash == h; i++) {
    s32 d = (s32)snd_zones[i].block - (s32)blk; d = d >= 0 ? d : 4 - d;
    if (d < bd) { bd = d; best = &snd_zones[i]; }
  }
  return best;
}

/* FNV-1a of the bytes that decide one sub-voice's timbre (gsnd/bake/zones.py builds the same): algorithm (+ feedback when operator 1
 * takes part), sub-voice number, and per operator of the sub-voice DT/MUL, and for modulators TL, KS/AR, DR, SR, SL/RR too */
static u32 sub_hash(const fmch *c, u32 vi) {
  u32 alg = c->alg, car = SVC[alg][vi], mask = SVM[alg][vi];
  u32 h = 0x811c9dc5u;
#define HB(b) h = (h ^ (u32)(b)) * 0x01000193u
  HB(alg | (((mask & 1u) ? c->fb : 0u) << 3)); HB(vi);
  for (u32 i = 0; i < 4; i++) {
    if (!((mask >> i) & 1u)) continue;
    const u8 *r = c->op[i];
    HB(r[0]);
    if (i != car) { HB(r[1] & 0x7f); HB(r[2]); HB(r[3] & 0x7f); HB(r[4]); HB(r[5]); }
  }
#undef HB
  return h;
}

/* phase step (of the mixer, 32 bit = one cycle) of a voice whose chip phase step is ps / rho; 131,700 (10.10 units) is the mixer's Nyquist */
static const u8 nz_lvl[16] = {0, 2, 3, 4, 6, 8, 11, 16, 23, 32, 45, 64, 91, 128, 181, 255};
#include "gsnd/gsnd_rho_recip.h"
static inline u32 rho_divrem(u32 n,u32 d,u32 inv,u32 *rem) {
  u32 q=(u32)(((u64)n*inv)>>32),r=n-q*d;
  if(r>=d){q++;r-=d;}
  *rem=r;return q;
}

static void ch_steps(fmch *c) {
  for (u32 vi = 0; vi < c->nv; vi++) {
    svoice *v = &c->v[vi];
    u32 ps = c->pstep[v->car];
    if (v->pure == SV_SINE) {                              /* a plain operator: it aliases at the chip's rate like the chip does */
      ps &= 0xFFFFFu; if (ps > 0x80000u) ps = 0x100000u - ps;
      if (ps > 131700u) { v->step = 0; v->pure = SV_MUTE; } else v->step = ps * 16308u;
    } else if (v->pure == SV_TABLE) {
      if (ps == v->sps && v->step) continue;              /* same pitch as last time (most recaches are level / envelope changes) */
      u32 r=v->zone->rho,rem=0,inv=rho_recip[r];
      if(r>1)ps=rho_divrem(ps,r,inv,&rem);
      if (ps > 131700u) { v->step = 0; v->pure = SV_MUTE; }
      else {u32 unused;v->step=ps*16308u+(r>1?rho_divrem(rem*16308u,r,inv,&unused):0);v->sps=c->pstep[v->car];}
    }
  }
}

HOT static void ch_recache(fmch *c) {
  u32 bf = c->bf;
  u32 keycode = (((bf >> 10) & 0xf) << 1) | ((0xfe80u >> ((bf >> 7) & 0xf)) & 1);
  u32 fnum = (bf & 0x7ff) << 1, block = (bf >> 11) & 7;
  u32 base = (fnum << block) >> 2;
  for (u32 i = 0; i < 4; i++) {
    const u8 *r = c->op[i];
    u32 dt = (r[0] >> 4) & 7, mul = r[0] & 15;
    s32 det = gfm_detune[keycode * 4 + (dt & 3)];
    if (dt & 4) det = -det;
    u32 m2 = mul * 2; if (!m2) m2 = 1;
    c->pstep[i] = ((((base + (u32)det) & 0x1ffffu) * m2) >> 1);
    if (!((CARR[c->alg] >> i) & 1)) continue;             /* modulators have no envelope here (it is baked into the snapshots) */
    eop *o = &c->e[i];
    o->tl8 = (u16)((r[1] & 0x7f) << 3);
    u32 sl = r[5] >> 4; sl |= (sl + 1) & 0x10;
    o->sl = (u16)(sl << 5);
    u32 ksr = keycode >> ((r[2] >> 6) ^ 3);
    o->rate[EG_ATK] = (u8)eg_eff_rate((r[2] & 0x1f) * 2, ksr);
    o->rate[EG_DEC] = (u8)eg_eff_rate((r[3] & 0x1f) * 2, ksr);
    o->rate[EG_SUS] = (u8)eg_eff_rate((r[4] & 0x1f) * 2, ksr);
    o->rate[EG_REL] = (u8)eg_eff_rate((r[5] & 0xf) * 4 + 2, ksr);
    o->next_ev = eg_next(o, cur_tick + 1);
  }
  ch_steps(c);
  c->dirty = 0;
}

#ifndef MD_HOST
__attribute__((section(".zaot.cold"),target("thumb"),noinline))
#endif
static void ch_keyon(fmch *c, u32 mask) {
  ch_recache(c);
  u32 blk = (c->bf >> 11) & 7, alg = c->alg;
  c->nv = NSV[alg];
  for (u32 vi = 0; vi < c->nv; vi++) {
    svoice *v = &c->v[vi];
    u32 car = SVC[alg][vi], msk = SVM[alg][vi];
    v->car = (u8)car; v->ph = 0; v->zone = 0; v->pure = SV_SINE; v->step = 0; v->sps = 0;
    if ((msk & (msk - 1)) || (car == 0 && c->fb)) {       /* a carrier with modulators (or self-feedback): baked table */
      u32 h = sub_hash(c, vi);
      const snd_zone *zn = zlookup(h, blk);
      if (zn) {
        u32 ci=(u32)(c-FM);
        voice_metadata[ci][vi].zone=*zn;
        for(u32 st=0;st<8;st++){
          voice_meta *meta=&voice_metadata[ci][vi];u32 ea=snd_tabs[zn->tab[st]];
#ifndef MD_HOST
          meta->tabs[st]=(u32)(cp20_waves+((ea&~3u)-CP20_WAVE_BASE));
#else
          meta->tabs[st]=ea&~3u;
#endif
          meta->shift[st]=(u8)(27u-(ea&3u));
          meta->gain5[st]=(u16)(5u*zn->gain[st]);
          meta->noise5[st]=(u16)(5u*nz_lvl[(zn->nz[st>>1]>>((st&1u)*4u))&15u]);
        }
        zn=&voice_metadata[ci][vi].zone;v->zone = zn; v->pure = zn->rho ? SV_TABLE : SV_NOISE; v->nph = 0x2545F491u + (vi + 1) * 0x9E3779B9u; if (!zn->rho) v->ph = v->nph; } else {
        snd_stats.misses++;
#ifdef SND_DEBUG
        fprintf(stderr, "miss hash %08x sub %u blk %u alg %u fb %u:", h, vi, blk, alg, c->fb);
        for (u32 i = 0; i < 4; i++) { fprintf(stderr, " |"); for (u32 k = 0; k < 6; k++) fprintf(stderr, "%02x", c->op[i][k]); }
        fprintf(stderr, "\n");
#endif
      }
    }
  }
  ch_steps(c);
  c->age = 0; c->active = 1;
  for (u32 i = 0; i < 4; i++) if (((CARR[alg] >> i) & 1) && ((mask >> i) & 1)) eg_key_on(&c->e[i], cur_tick);
  snd_stats.keyons++;
}

static void ym_apply(u32 part, u32 reg, u32 val) {
  if (reg < 0x30) {
    if (reg != 0x28) return;
    u32 chn = val & 7;
    if ((chn & 3) == 3) return;
    fmch *c = &FM[(chn & 3) + ((chn >> 2) & 1) * 3];
    u32 mask = val >> 4, old = c->kon;
    c->kon = (u8)mask;
    if (mask && !old) ch_keyon(c, mask);
    else if (old) {
      if (c->dirty) ch_recache(c);
      for (u32 i = 0; i < 4; i++) {
        if (!((CARR[c->alg] >> i) & 1)) continue;
        eop *o = &c->e[i]; u32 on = (mask >> i) & 1;
        if (on != o->key) { if (on) eg_key_on(o, cur_tick); else eg_key_off(o, cur_tick); }
      }
    }
    return;
  }
  u32 slot = reg & 3;
  if (reg < 0xa0) {
    if (slot == 3) return;
    u32 row = (reg >> 4) - 3;
    if (row > 5) return;
    fmch *c = &FM[slot + 3 * part];
    u32 opi = OP_OF[(reg >> 2) & 3];
    c->op[opi][row] = (u8)val; c->dirty = 1;
    return;
  }
  if (reg < 0xb0) {
    if (slot == 3) return;
    u32 bank = (reg >> 3) & 1;
    if (reg & 4) { latch[bank] = (u8)(val & 0x3f); return; }
    if (bank) return;
    fmch *c = &FM[slot + 3 * part];
    c->bf = (u16)((latch[0] << 8) | val); c->dirty = 1;
    return;
  }
  if (reg < 0xb8) {
    if (slot == 3) return;
    fmch *c = &FM[slot + 3 * part];
    if (!(reg & 4)) { c->alg = (u8)(val & 7); c->fb = (u8)((val >> 3) & 7); }
    else c->pan = (u8)((val >> 6) & 3);
  }
}

/* ------------------------------------------------------------------ PSG (SN76489) */
#include "gsnd/psg_step_lut.h"
static void psg_retune(u32 i) {                           /* tone i: square toggling every `period` ticks of clock/16 */
  /* p is the original 10-bit SN76489 period (0..1023).  The host-generated
   * LUT preserves all bits of the original 64-bit formula, including the
   * p<9 muted range, while avoiding software division in the sound driver. */
  psg_step[i] = psg_step_lut[psg_per[i]&1023u];
}
static void psg_noise_retune(void) {
  u32 n = psg_noise & 3;
  psg_nstep = n < 3 ? (2244120000u >> n) : psg_step[2];   /* shift rate 223722/2/(16<<n) Hz, or the frequency of tone 2 */
}
static void psg_write(u32 v) {
  if (v & 0x80) {
    psg_latch = (u8)((v >> 4) & 7);
    u32 ch = psg_latch >> 1;
    if (psg_latch & 1) psg_vol[ch] = (u8)(v & 15);
    else if (ch < 3) { psg_per[ch] = (u16)((psg_per[ch] & 0x3f0) | (v & 15)); psg_retune(ch); if (ch == 2) psg_noise_retune(); }
    else { psg_noise = (u8)(v & 7); psg_lfsr = 0x8000; psg_noise_retune(); }
  } else {
    u32 ch = psg_latch >> 1;
    if (psg_latch & 1) psg_vol[ch] = (u8)(v & 15);
    else if (ch < 3) { psg_per[ch] = (u16)(((v & 0x3f) << 4) | (psg_per[ch] & 15)); psg_retune(ch); if (ch == 2) psg_noise_retune(); }
    else { psg_noise = (u8)(v & 7); psg_lfsr = 0x8000; psg_noise_retune(); }
  }
}
static void psg_render(s32 *acc, u32 n) {                /* n samples into acc[0..n) */
  for (u32 ch = 0; ch < 3; ch++) {
    u32 a = psg_amp[psg_vol[ch]];
    if (!psg_step[ch]) continue;
    u32 ph = psg_ph[ch], st = psg_step[ch];
    if (a) for (u32 i = 0; i < n; i++) { acc[i] += (ph & 0x80000000u) ? (s32)a : -(s32)a; ph += st; }
    else ph += st * n;
    psg_ph[ch] = ph;
  }
  { u32 a = psg_amp[psg_vol[3]];
    u32 ph = psg_nph, st = psg_nstep; u16 l = psg_lfsr;
    if (!st) {
      /* No noise clocks: phase and LFSR stay unchanged. */
      if (a) for (u32 i = 0; i < n; i++) acc[i] += (l & 1u) ? (s32)a : -(s32)a;
    } else if (a) {
      for (u32 i = 0; i < n; i++) {
        u32 np = ph + st;
        if (np < ph) { u32 fb = (psg_noise & 4) ? ((l ^ (l >> 3)) & 1u) : (l & 1u); l = (u16)((l >> 1) | (fb << 15)); }
        ph = np;
        acc[i] += (l & 1u) ? (s32)a : -(s32)a;
      }
    } else {                                              /* silent: only the shift register advances */
      /* The accumulator wraps at most once per output sample.  Count those
       * wraps exactly with ARM7's 32x32->64 multiply instead of iterating
       * over silent output samples.  This changes neither phase nor LFSR. */
      uint64_t sum = (uint64_t)ph + (uint64_t)st * n;
      u32 clocks = (u32)(sum >> 32);
      ph = (u32)sum;
      while (clocks--) {
        u32 fb = (psg_noise & 4) ? ((l ^ (l >> 3)) & 1u) : (l & 1u);
        l = (u16)((l >> 1) | (fb << 15));
      }
    }
    psg_nph = ph; psg_lfsr = l; }
}

/* ------------------------------------------------------------------ FM rendering */
void snd_mix1(s32 *acc, u32 n, u32 *pph, u32 step, const s8 *tab, s32 w, u32 sh);
void snd_mix2(s32 *acc, u32 n, u32 *pph, u32 step, const s8 *ta, const s8 *tb, s32 wa, s32 wb, u32 sha, u32 shb);
void snd_mixn(s32 *acc, u32 n, u32 *plfsr, s32 w);
void snd_out256(const s32 *m, const s32 *l, const s32 *r, s8 *oa, s8 *ob, u32 n);
void snd_outmono(const s32 *m, s8 *oa, s8 *ob, u32 n);


/* Native stage-tree: constant comparisons rather than seven-index loop.
   All integer interpolation and stage boundaries reproduce the original. */
static inline void stage_of(u32 age, u32 *s, u32 *f) {
  if(age<=80u){*s=0;*f=0;return;}
  u32 k;
  if(age<3210u){
    if(age<670u) k=(age<270u)?0u:1u;
    else k=(age<1470u)?2u:3u;
  }else{
    if(age<14700u) k=(age<6960u)?4u:5u;
    else if(age<32100u) k=6u;
    else {*s=7u;*f=0u;return;}
  }
  *s=k;*f=((age-ST_T[k])*st_recip[k])>>16;
}

/* one chunk of n <= BLK samples starting at frame position p, into the chunk accumulators m / l / r (index 0 = position p) */
HOT __attribute__((noinline)) static void ch_render(fmch *c, u32 t1, u32 n, s32 *m, s32 *l, s32 *r) {
  s32 *acc = c->pan == 3 ? m : c->pan == 2 ? l : c->pan == 1 ? r : 0;
    /* CP48: all channels in a chunk share one precomputed envelope deadline. */
    u32 s, f; stage_of(c->age, &s, &f);
    u32 s2 = s < 7 ? s + 1 : 7;
    for (u32 vi = 0; vi < c->nv; vi++) {
      svoice *v = &c->v[vi];
      eop *o = &c->e[v->car];
      eg_advance(o, t1);
      if (acc && o->env <= 0x380) {
        u32 att = o->env + o->tl8; if (att > 0x3ff) att = 0x3ff;
        s32 A = gain_tab[att];
        
        if (v->pure == SV_SINE) snd_mix1(acc, n, &v->ph, v->step, sine8, A, 24);
        else if (v->pure == SV_MUTE) { }
        else if (v->pure == SV_NOISE) {                    /* aliasing buzz of the chip: noise at the measured level */
          const voice_meta *meta=(const voice_meta *)v->zone;
          s32 wa = (((A * (s32)meta->gain5[s]) >> 10) * (s32)(256 - f)) >> 8, wb = (((A * (s32)meta->gain5[s2]) >> 10) * (s32)f) >> 8;
          snd_mixn(acc, n, &v->ph, wa + wb);
        } else {
          const voice_meta *meta=(const voice_meta *)v->zone;
          u32 ea=meta->tabs[s],eb=meta->tabs[s2];
          u32 sha=meta->shift[s],shb=meta->shift[s2];
#ifndef MD_HOST
          const s8 *ta=(const s8 *)ea,*tb=(const s8 *)eb;
#else
          const s8 *ta=(const s8 *)(snd_data0+ea),*tb=(const s8 *)(snd_data0+eb);
#endif
          s32 wa = (((A * (s32)meta->gain5[s]) >> 10) * (s32)(256 - f)) >> 8, wb = (((A * (s32)meta->gain5[s2]) >> 10) * (s32)f) >> 8;   /* stage gain unit = 1.25/256 of a full carrier */
          if (ta == tb) snd_mix1(acc, n, &v->ph, v->step, ta, wa + wb, sha);
          else if (!wb) snd_mix1(acc, n, &v->ph, v->step, ta, wa, sha);
          else if (!wa) snd_mix1(acc, n, &v->ph, v->step, tb, wb, shb);
          else snd_mix2(acc, n, &v->ph, v->step, ta, tb, wa, wb, sha, shb);
          { u32 na=meta->noise5[s],nb=meta->noise5[s2];
            if (na | nb) {                                 /* the part of the timbre the table cannot carry */
              s32 wn = ((((A * (s32)na) >> 10) * (s32)(256 - f)) + (((A * (s32)nb) >> 10) * (s32)f)) >> 8;
              snd_mixn(acc, n, &v->nph, wn);
            } }
        }
      } else v->ph += v->step * n;
    }
    if (c->kon) c->age += n;
}
static void ch_settle(fmch *c) {
  if (!c->kon) {                                          /* released and silent: stop spending time on this channel */
    u32 live = 0;
    for (u32 vi = 0; vi < c->nv; vi++) if (c->e[c->v[vi].car].env <= 0x380) live = 1;
    if (!live) c->active = 0;
  }
}

/* The segment is rendered in chunks of BLK samples into small accumulators on the stack (IWRAM: single-cycle access), each
 * chunk mixed down to the output at once.  Chunks start at the segment start, like the channels' envelope / stage blocks. */
static void render_segment(u32 p0, u32 p1, s8 *out_l, s8 *out_r) {
  s32 m[BLK], l[BLK], r[BLK];
  /* CP50: only left/right FM pans can modify the side buffers. */
  u32 mono = 1;
  for (u32 c = 0; c < 6; c++)
    if (FM[c].active && (FM[c].pan == 1 || FM[c].pan == 2)) mono = 0;
  for (u32 p = p0; p < p1;) {
    u32 n = p1 - p; if (n > BLK) n = BLK;
    if (mono) { for (u32 i = 0; i < n; i++) m[i] = 0; }
    else { for (u32 i = 0; i < n; i++) m[i] = l[i] = r[i] = 0; }
    u32 t1 = tick_at(p + n);
    for (u32 i = 0; i < 6; i++) if (FM[i].active) ch_render(&FM[i], t1, n, m, l, r);
    if (!snd_nopsg) psg_render(m, n);
    if (mono) snd_outmono(m, out_l + p, out_r + p, n);
    else snd_out256(m, l, r, out_l + p, out_r + p, n);
    p += n;
  }
  for (u32 i = 0; i < 6; i++) if (FM[i].active) ch_settle(&FM[i]);
}

/* ------------------------------------------------------------------ Z80 control */
static void ym_reset(void) {
  for (u32 i = 0; i < 6; i++) {
    fmch *c = &FM[i];
    u8 *b = (u8 *)c; for (u32 k = 0; k < sizeof *c; k++) b[k] = 0;
    c->pan = 3;
    for (u32 k = 0; k < 4; k++) { c->e[k].env = 0x3ff; c->e[k].state = EG_REL; c->e[k].next_ev = GSND_NEVER; }
  }
  latch[0] = latch[1] = 0;
  ym_r24 = ym_r25 = ym_r26 = ym_r27 = ym_status = tA_run = tB_run = 0; ym_addr[0] = ym_addr[1] = 0;
}
static void psg_reset(void) {
  for (u32 i = 0; i < 3; i++) { psg_per[i] = 0; psg_vol[i] = 15; psg_ph[i] = 0; psg_step[i] = 0; }
  psg_vol[3] = 15; psg_latch = 0; psg_noise = 0; psg_lfsr = 0x8000; psg_nph = 0; psg_nstep = 0;
}

void mdsnd_init(void) {
#ifndef MD_HOST
  cp20_wave_init();
#endif
  for (u32 i = 0; i < 1024; i++) {                        /* attenuation (4.6) -> weight; the log domain of the OPN2 is 4.8 */
    u32 a = i << 2; u32 g = gfm_pow[a & 255] >> (a >> 8);
    gain_tab[i] = (u16)((g * 4031u + 500u) / 1000u);
  }
  for (u32 i = 0; i < 256; i++) {                         /* the OPN2 sine (log-sin ROM through the exponent ROM), -127..127 */
    u32 idx = i << 2, q = (idx & 0x100) ? ~idx : idx;
    u32 att = gfm_sin_att[q & 0xff];
    u32 v = gfm_pow[att & 255] >> (att >> 8);
    s32 y = (s32)((v * 127u + 4084u) / 8168u);
    sine8[i] = (s8)((idx >> 9) & 1 ? -y : y);
  }
  for (u32 i = 0; i < 7; i++) st_recip[i] = (256u << 16) / (ST_T[i + 1] - ST_T[i]);
  for (u32 v = 0; v < 15; v++) {                          /* 2 dB per step; tone peak ~ 20/127 of full scale */
    static const u32 tab[15] = {1310720, 1041195, 827087, 657000, 521879, 414541, 329291, 261569, 207770, 165038, 131089, 104127, 82711, 65700, 52188};
    psg_amp[v] = tab[v];
  }
  psg_amp[15] = 0;
  ym_reset(); psg_reset();
  z_run = z_rst = z_busreq = z_skipok = 0; z_bank = 0; z_hash_done = 0; nev = 0; g_tick = 0;
  { u8 *b = (u8 *)&snd_stats; for (u32 i = 0; i < sizeof snd_stats; i++) b[i] = 0; }
}

static u32 driver_hash(void) {
  u32 h = 0x811c9dc5u;
  for (u32 i = 0; i < 4070u; i++) h = (h ^ md_zram[i]) * 0x01000193u;
  return h;
}

static u32 driver_hash(void);
void mdsnd_busreq(u32 v) {
  /* the 68000 may rewrite the Z80 RAM while it owns the bus: look at the driver again when it gives the bus back */
  if (v & 1u) { if (!z_busreq) z_busreq_wr = M.n_z80_wr; }
  else if (z_busreq && z_busreq_wr != M.n_z80_wr && aot_ok) aot_ok = driver_hash() == DRIVER_HASH;
  z_busreq = (u8)(v & 1u);
}
void mdsnd_reset(u32 v) {
  if (!(v & 1u)) { z_rst = 1; return; }
  if (!z_rst) return;
  z_rst = 0;
  { Z80F n; z80f_reset(&n); z = n; }
  z.sp = 0xFFFF; z.pc = 0; z.iff1 = z.iff2 = 0; z_bank = 0; z_run = 1;
  z_skipok = driver_hash() == DRIVER_HASH;
  aot_ok = z_skipok; aot_dead = 0; t_last = 0; z_fnext = 0;
#ifdef MD_HOST
  if (snd_noaot) aot_ok = 0;
  if (snd_noskip) z_skipok = 0;
#endif
  ym_reset(); psg_reset();
}
void mdsnd_psg_68k(u32 v) { EV_HOOK(2, 0, v, z_fbase); ev_push(0, (2u << 16) | (v & 0xFFu)); }
void mdsnd_ym_68k(u32 a, u32 v) {
  u32 p = (a >> 1) & 1u;
  if (!(a & 1u)) ym_addr[p] = (u8)v;
  else {
    u32 reg = ym_addr[p];
    if (reg == 0x28 || reg >= 0x30) ev_push(0, (p << 16) | (reg << 8) | (v & 0xFFu));
  }
}

/* The driver's idle loop (the mailbox poll at 0x006F..0x0085 and its helper 0x0899..0x08B0) changes nothing but the R
 * register and one scratch byte while the mailbox is empty, so the time until the next interrupt / timer flag is skipped. */
static inline int idle_ready(u32 now) {
  timers_sync(now);
  u32 m7 = md_zram[0x1C07];
  if (m7) { if (!(m7 & 0x80u) || (ym_status & 3u)) return 0; }
  return !((md_zram[0x1C0A] | md_zram[0x1C0B] | md_zram[0x1C0C]) & 0x80u);
}
/* Whole iterations of the idle loop are skipped, from its start (0x0070, right after the EI at 0x006F), so the loop phase
 * stays exact: the frame interrupt is then taken at the same instruction as on the real Z80.  Mailbox-only loop: 214 T and
 * 25 refreshes per iteration.  Timer-polling loop (0x1C07 bit 7): 436 T, 51 refreshes, the status read starts 400 T in;
 * an iteration whose read would see a polled timer flag is not skipped.  Returns the number of iterations (0 = none). */
#define IDLE_L0 214u
#define IDLE_R0 25u
#define IDLE_L1 436u
#define IDLE_R1 51u
#define IDLE_RD1 400u
static u32 idle_iters(u32 now, u32 tend, u32 *len) {
  if ((s32)(tend - now) <= 0) return 0;
  u32 room = tend - now;
  if (!(md_zram[0x1C07] & 0x80u)) { *len = IDLE_L0; return room / IDLE_L0; }
  u32 k = room / IDLE_L1;
  if ((tA_run && (ym_r27 & 4)) || (tB_run && (ym_r27 & 8))) {
    s32 d = 0x7FFFFFFF;                                   /* T-states (from now) at which the first polled flag is set */
    if (tA_run && (ym_r27 & 4)) { s32 t = (tA_left + 4) / 5; if (t < d) d = t; }
    if (tB_run && (ym_r27 & 8)) { s32 t = (tB_left + 4) / 5; if (t < d) d = t; }
    /* iteration j (0-based) reads at now + 400 + 436 j and must read before the flag: 400 + 436 j < d */
    u32 kf = d <= (s32)IDLE_RD1 ? 0u : (u32)(d - (s32)IDLE_RD1 + (s32)IDLE_L1 - 1) / IDLE_L1;
    if (kf < k) k = kf;
  }
  *len = IDLE_L1; return k;
}
/* skips whole idle iterations from `now`; returns the T-states skipped, *refresh = R increments, *r17 = the R value the
 * last skipped iteration's LD A,R read (relative to R at `now`; its low 7 bits are stored at 0x1C17) */
static u32 idle_skip(u32 now, u32 tend, u32 *refresh, u32 *r17) {
  u32 len, k = idle_iters(now, tend, &len);
  if (!k) return 0;
  u32 lim = k * len;
  timers_sync(now + lim);
  snd_stats.z80_skips++; snd_stats.z80_tskip += lim;
  if (len == IDLE_L0) { *refresh = k * IDLE_R0; *r17 = (k - 1) * IDLE_R0 + 3u; }
  else { *refresh = k * IDLE_R1; *r17 = (k - 1) * IDLE_R1 + 29u; }
  return lim;
}
static inline int z_idle(void) {
  return z.pc == 0x70u && idle_ready(z.cyc);
}
static u32 z_skip(u32 tend) {
  u32 rf, r17, lim = idle_skip(z.cyc, tend, &rf, &r17);
  if (!lim) return 0;
  md_zram[0x1C17] = (u8)(((z.r + r17) & 0x7Fu) | (z.rhi & 0x80u));
  z.cyc += lim; z.r = (u8)(z.r + rf);
  return 1;
}

#ifdef SND_DEBUG
u32 snd_dbg(u32 k) { return k == 0 ? z.pc : k == 1 ? z_skipok : k == 2 ? z_run : k == 3 ? z_busreq : k == 4 ? z_rst : k == 5 ? ym_status : k == 6 ? z.int_pending : k == 7 ? z.iff1 : 0; }
#endif

/* ------------------------------------------------------------------ translated driver (z80aot.py) */
/* the translated driver and its memory helpers live in their own ROM region on the GBA (build_cp09.py: ZAOT_BASE) */
#ifdef MD_HOST
#define ZAOT
#define SND_ARM
#else
#define ZAOT __attribute__((section(".zaot"),target("thumb")))
#define SND_ARM __attribute__((target("arm")))
#endif
static inline u32 ard_io(u32 a, u32 ts) {                  /* 0x4000..0x7FFF */
  if (a < 0x6000u) { if ((a & 3u) == 0) { timers_sync(ts); return ym_status; } return 0u; }
  return 0xFFu;
}
static inline u32 romrd(u32 a) {
  u32 r = (z_bank << 15) | (a & 0x7FFFu);
  return r < m68k_romsize ? m68k_romp[r >> 12][r & 0xFFFu] : 0xFFu;
}
static void awr_io(u32 a, u32 v, u32 ts, u32 te) {
  if (a < 0x6000u) { u32 p = (a >> 1) & 1u; if (!(a & 1u)) ym_addr[p] = (u8)v; else ym_data(p, ym_addr[p], v, ts, te); return; }
  if (a < 0x6100u) { z_bank = (z_bank >> 1) | ((v & 1u) << 8); return; }
  if (a == 0x7F11u) { EV_HOOK(2, 0, v, te); ev_push(pos_of(te), (2u << 16) | v); return; }
}
#undef RD8
#undef WR8
HOT SND_ARM __attribute__((noinline)) static u32 ard(u32 a, u32 ts) {   /* reads outside Z80 RAM */
  return a >= 0x8000u ? romrd(a) : ard_io(a, ts);
}
HOT SND_ARM static u32 awr(u32 a, u32 v, u32 ts, u32 te);
#define RD8(a) ({ uint32_t _ra = (a) & 0xFFFFu; _ra < 0x4000u ? (uint32_t)md_zram[_ra & 0x1FFFu] : ard(_ra, TS); })
/* Native driver RAM writes and word stack accesses.  Only the unusual bus
 * cases use the general helpers; code writes retain the existing SMC guard. */
#define WR8(a, v) do { u32 _ca=(a)&0xFFFFu,_cv=(v); \
  if(_ca-0x1000u<0x3000u)md_zram[_ca&0x1FFFu]=(u8)_cv; \
  else smc|=awr(_ca,_cv,TS,TE); \
}while(0)
typedef u16 __attribute__((may_alias)) ZNativeWord;
#undef PUSH16
#undef POP16
#define PUSH16(v) do { u32 _cpv=(v);SP=(SP-2u)&0xFFFFu; \
  if(SP-0x1000u<0x3000u&&!(SP&1u))*(ZNativeWord*)(md_zram+(SP&0x1FFFu))=(u16)_cpv; \
  else {WR8((SP+1u)&0xFFFFu,_cpv>>8);WR8(SP,_cpv&255u);} \
}while(0)
#define POP16() ({ u32 _crv; \
  if(SP<0x4000u&&!(SP&1u))_crv=*(const ZNativeWord*)(md_zram+(SP&0x1FFFu)); \
  else _crv=RD8(SP)|(RD8((SP+1u)&0xFFFFu)<<8); \
  SP=(SP+2u)&0xFFFFu;_crv; \
})
/* the idle skip is tried right after the main loop's EI (the translated code calls this) */
#define IDLE_HOOK(nextpc) do { if (z_skipok && !z->int_pending && idle_ready(cyc)) { uint32_t _rf, _r17; \
    uint32_t _lim = idle_skip(cyc, tend, &_rf, &_r17); \
    if (_lim) { md_zram[0x1C17] = (u8)(((R + _r17) & 0x7Fu) | (z->rhi & 0x80u)); cyc += _lim; R += _rf; } } } while (0)
#ifdef MD_HOST
FILE *snd_trace;                                           /* host tests: per-instruction state log (reference vs translated code) */
static void trace_rec(u32 pc, u32 cyc, u32 a, u32 f, u32 b, u32 c, u32 d, u32 e, u32 h, u32 l, u32 sp, u32 ix, u32 iy) {
  if (snd_trace) fprintf(snd_trace, "%04x %u %02x %02x %02x %02x %02x %02x %02x %02x %04x %04x %04x\n", pc, cyc - z_fbase, a, f, b, c, d, e, h, l, sp, ix, iy);
}
#include <stdlib.h>
static u32 snd_pccount[0x1000];
static void pchist_dump(void) { const char *f = getenv("SND_PCHIST"); FILE *o = f ? fopen(f, "a") : 0; if (!o) return;
  for (u32 i = 0; i < 0x1000; i++) if (snd_pccount[i]) fprintf(o, "%04x %u\n", i, snd_pccount[i]); fclose(o); }
#define Z80TRACE(pc, pt) do { if (!snd_pccount[0xFFF]++) atexit(pchist_dump); snd_pccount[(pc) & 0xFFF]++; trace_rec((pc), cyc + (pt), A, F, B, C, D, E, H, L, SP, IX, IY); } while (0)
#else
#define Z80TRACE(pc, pt) ((void)0)
#endif
#define Z80T(pc, pt, pr) Z80TRACE(pc, pt)
#include "gsnd/z80aot/native_driver.h"
#include "gsnd/z80aot/z80drv.inc"
/* writes (out of line: the translated code stays small); returns 1 when the write hit translated code */
HOT SND_ARM __attribute__((noinline)) static u32 awr(u32 a, u32 v, u32 ts, u32 te) {
  if (a < 0x4000u) { md_zram[a & 0x1FFFu] = (u8)v; return a < 0x1000u ? (z80drv_code[a >> 3] >> (a & 7u)) & 1u : 0u; }
  awr_io(a, v, ts, te); return 0;
}
#include "gsnd/z80aot/native_driver_impl.h"
#undef RD8
#undef WR8
#define RD8(a) zrd(a)
#define WR8(a, v) zwr((a), (v))

static inline int z80drv_known(u32 pc) { return pc < 0x1000u && ((z80drv_lead[pc >> 3] >> (pc & 7u)) & 1u); }
static void accept_int(void) {                                   /* IM 1 */
  z.iff1 = z.iff2 = 0; z.int_pending = 0; z.r++;
  z.sp = (u16)(z.sp - 2u); zwr((u32)z.sp + 1u, (u32)z.pc >> 8); zwr(z.sp, z.pc & 255u);
  z.pc = 0x38; z.cyc += 13;
}

static void accept_int(void);
#ifdef MD_HOST
extern FILE *snd_trace;
static void trace_rec(u32 pc, u32 cyc, u32 a, u32 f, u32 b, u32 c, u32 d, u32 e, u32 h, u32 l, u32 sp, u32 ix, u32 iy);
#endif
static void z80_slices(u32 tend) {
#ifdef MD_HOST
  if (snd_slice == 1) {                         /* reference: one instruction at a time, interrupt checked before every instruction */
    u32 skip_int = 0;
    while ((s32)(tend - z.cyc) > 0) {
      if (z.int_pending && z.iff1 && !skip_int) accept_int();
      u32 op = z.pc < 0x2000u ? md_zram[z.pc] : 0u; skip_int = op == 0xFBu;
      if (snd_trace) trace_rec(z.pc, z.cyc, z.a, z.f, z.b, z.c, z.d, z.e, z.h, z.l, z.sp, z.ix, z.iy);
      z80f_run(&z, z.cyc + 1);
      snd_stats.z80_slices++;
      timers_sync(z.cyc);
    }
    return;
  }
#endif
  while ((s32)(tend - z.cyc) > 0) {
    if (z_skipok && !z.int_pending && z_idle() && z_skip(tend)) continue;
    u32 c0 = z.cyc, tg = c0 + SLICE_LEN;
    if ((s32)(tg - tend) > 0) tg = tend;
    z80f_run(&z, tg);
    snd_stats.z80_slices++; snd_stats.z80_texec += z.cyc - c0;
    timers_sync(z.cyc);
  }
}

static void z80_aot(u32 tend) {
  if (z.iff1 && !z.halted) accept_int();
  while ((s32)(tend - z.cyc) > 0) {
    u32 st = z80drv_run(&z, tend);
    snd_stats.z80_slices++;
    if (st == 0) break;
    if (st == 1) { u32 c0 = z.cyc;
#ifdef MD_HOST
      if (snd_trace) trace_rec(z.pc, z.cyc, z.a, z.f, z.b, z.c, z.d, z.e, z.h, z.l, z.sp, z.ix, z.iy);
#endif
      z80f_run(&z, z.cyc + 1); snd_stats.z80_texec += z.cyc - c0; timers_sync(z.cyc); if (z.int_pending && z.iff1) accept_int(); }
    else if (st == 4) accept_int();
    else if (st == 3) { aot_dead = 1; z80_slices(tend); break; }           /* the driver changed its own code */
    else {                       /* not translated, or the frame ends inside the next run: interpret up to translated code */
      while ((s32)(tend - z.cyc) > 0) {
        u32 c0 = z.cyc;
#ifdef MD_HOST
        if (snd_trace) trace_rec(z.pc, z.cyc, z.a, z.f, z.b, z.c, z.d, z.e, z.h, z.l, z.sp, z.ix, z.iy);
#endif
        z80f_run(&z, z.cyc + 1); snd_stats.z80_texec += z.cyc - c0; timers_sync(z.cyc);
        if (z.int_pending && z.iff1) accept_int();
        if (z80drv_known(z.pc)) break;
      }
    }
  }
}

static void z80_frame(void) {
  if (!z_run || z_busreq || z_rst) { z_fnext = z.cyc; z_fbase = z.cyc; return; }
  z_fbase = z_fnext;
  u32 tend = z_fbase + SND_FRAME_T;
  z_fnext = tend;
  snd_stats.z80_frames++;
  z.int_pending = 1; z.int_data = 0xFF;
  if (aot_ok && !aot_dead && z.im == 1) z80_aot(tend); else z80_slices(tend);
}

/* ------------------------------------------------------------------ one frame */
/* CP11: move the unchanged synthesis/envelope path to IWRAM. */
HOT void mdsnd_frame(s8 *out_l, s8 *out_r) {
#ifndef MD_HOST
  u32 tk0 = md_ticks();
#endif
  z80_frame();
#ifndef MD_HOST
  u32 tk1 = md_ticks();
#endif
  if (nev > snd_stats.max_events) snd_stats.max_events = nev;
  snd_stats.events += nev;
  u32 p = 0, ei = 0;
  for (;;) {
    cur_tick = tick_at(p);
    while (ei < nev && ev_pos[ei] <= p) {
      u32 d = ev_d[ei++], kind = d >> 16;
      if (kind == 2) psg_write(d & 0xFFu); else ym_apply(kind, (d >> 8) & 0xFFu, d & 0xFFu);
    }
    for (u32 i = 0; i < 6; i++) if (FM[i].dirty) ch_recache(&FM[i]);
    if (p >= SND_N) break;
    u32 next = ei < nev ? ev_pos[ei] : SND_N;
    if (next > SND_N) next = SND_N;
    if (next > p) { render_segment(p, next, out_l, out_r); p = next; }
  }
  g_tick += 296;
  nev = 0;
#ifndef MD_HOST
  { u32 tk2 = md_ticks(); snd_tz = tk1 - tk0; snd_tr = tk2 - tk1; if (snd_tz > snd_tz_max) snd_tz_max = snd_tz; if (snd_tr > snd_tr_max) snd_tr_max = snd_tr; }
#endif
}
