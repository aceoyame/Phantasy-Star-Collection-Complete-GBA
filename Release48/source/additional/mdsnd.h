/* Genesis sound for the text adventures (CP09).
 *
 * The games' own Z80 sound driver runs on the PSC8 Z80 core (idle loops skipped).  Its YM2612 writes drive FM channels
 * that play baked wavetable snapshots of each patch (the PSIV technique: per patch and octave a few timbre snapshots,
 * cross-faded over the first seconds of a note, with the real carrier envelopes); the SN76489 writes drive three square
 * channels and a noise channel.  One call renders one GBA frame (SND_N samples at 13,379 Hz). */
#pragma once
#include "md.h"

#define SND_N 224                       /* samples per GBA frame: 13,379 Hz */
#define SND_FRAME_T 59659u              /* Z80 T-states per sound frame */

void mdsnd_init(void);                  /* after md_init_machine: tables, bank, Z80 held in reset */
void mdsnd_busreq(u32 v);               /* 68000 writes to 0xA11100 (bit 0 = request the Z80 bus) */
void mdsnd_reset(u32 v);                /* 68000 writes to 0xA11200 (bit 0 = 0 asserts reset, 1 releases) */
void mdsnd_psg_68k(u32 v);              /* 68000 writes to the PSG port (0xC00011) */
void mdsnd_ym_68k(u32 a, u32 v);        /* 68000 writes straight to the YM2612 ports (0xA04000..3) */
void mdsnd_frame(s8 *out_l, s8 *out_r); /* run the sound CPU for one frame and render SND_N stereo samples */

/* data supplied by the generated bank (gsnd_bank.c) */
typedef struct {
  u32 hash;                             /* FNV-1a of the sub-voice's timbre bytes (see sub_hash; carrier level and envelope excluded) */
  u8  block, rho, pad0, pad1;           /* octave block, carrier cycles per table cycle */
  u8  gain[8];                          /* peak of each stage, in units of 1.25/256 of a full-scale carrier */
  u16 tab[8];                           /* stage -> index into snd_tabs[] */
  u8  nz[4];                            /* noise supplement of each stage (4 bits, two stages per byte): what the table does not carry */
} snd_zone;
extern const u32 snd_tabs[];            /* table address (relative to snd_data0) | log2(length) - 5 */
extern const u32 snd_ntabs;
extern const snd_zone snd_zones[];
extern const u32 snd_nzones;
extern const unsigned char *snd_data0;  /* base added to the table addresses: 0 on the GBA (absolute ROM addresses) */

/* instrumentation (host tests) */
typedef struct { u32 keyons, misses, z80_slices, z80_skips, events, max_events, dropped, z80_texec, z80_tskip, z80_frames; } SndStats;
extern SndStats snd_stats;
