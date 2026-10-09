/* sms_host: host-side SMS / Game Gear machine around the verified C core (core/z80f.h), for analysis of what a game
 * does (hot Z80 PCs, port traffic, cartridge-SRAM writes) without the GBA.  It mirrors psc8.c's memory map, mapper,
 * VDP port semantics, frame/line interrupt scheduling and key mapping; graphics and audio are not produced.
 *
 *   gcc -O2 -o sms_host tools/sms_host.c        (run from psc8/)
 *   sms_host ROM gg|sms FRAMES [pcs N] [script bench|none|monkey] [seed S] [from FRAME] [heavy FRACTION]
 *   (monkey = random button presses, to reach gameplay; statistics only from FRAME on; "heavy" frames = Z80 busy fraction >= FRACTION) [from FRAME]   (statistics only from FRAME on)
 *
 * Output: frame count, SRAM write statistics, port IN counts, and the N hottest PCs by T-states.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static uint8_t *cart; static unsigned cart_size, cart_mask;
static uint8_t ram[8192], sram[32768], vram[16384], cram[64], vr[16], banks[4];
static uint8_t status, cmd_first, cmd_lo, vcode, readbuf, mode_gg; static unsigned vaddr, scanline, keys;
static uint32_t line0, zcyc_now;                       /* cycle at the start of the frame, current cycle */
static unsigned long sram_wr, sram_max; static unsigned long port_in[256], port_out[256];
static uint32_t cyc_hist[65536]; static uint32_t cnt_hist[65536];
static uint64_t op_hist[5][256]; static int op_dump;   /* 0 plain, 1 ED, 2 DD/FD, 3 CB, 4 DD/FD CB */
static uint32_t frame_hist[65536], heavy_hist[65536]; static unsigned busy_bins[11]; static unsigned long heavy_frames, all_frames, instr_busy, instr_all;

static uint8_t rd8(unsigned a) {
  a &= 0xffff;
  if (a < 0x400) return cart[a];
  if (a < 0x4000) return cart[(((unsigned)banks[1] << 14) & cart_mask) + a];
  if (a < 0x8000) return cart[(((unsigned)banks[2] << 14) & cart_mask) + (a - 0x4000)];
  if (a < 0xc000) { if (banks[0] & 8) return sram[(a & 16383) + ((banks[0] & 4) ? 16384 : 0)]; return cart[(((unsigned)banks[3] << 14) & cart_mask) + (a - 0x8000)]; }
  return ram[a & 8191];
}
static void wr8(unsigned a, unsigned v) {
  a &= 0xffff;
  if (a >= 0xc000) { ram[a & 8191] = (uint8_t)v; if (a >= 0xfffc) banks[a & 3] = (uint8_t)v; return; }
  if (a >= 0x8000 && (banks[0] & 8)) { unsigned off = (a & 16383) + ((banks[0] & 4) ? 16384 : 0); sram[off] = (uint8_t)v; sram_wr++; if (off > sram_max) sram_max = off; }
}
static uint32_t cyc_cur;
static uint8_t port_rd(unsigned p) {
  p &= 255; port_in[p]++;
  if (p == 0xf2 && !mode_gg) return 2;                  /* FM unit not present, as psc8.c reports it without YM2413 */
  if (mode_gg && p < 7) { switch (p) { case 0: return (keys & 8) ? 0x40 : 0xc0; case 1: case 4: return 0x7f; case 2: case 5: case 6: return 0xff; default: return 0; } }
  if (p >= 0x40 && p < 0x80) { if (!(p & 1)) return scanline <= 0xda ? scanline : scanline - 6; return ((cyc_cur - line0) % 228) >> 1; }
  if (p >= 0x80 && p < 0xc0) { cmd_first = 0; if (p & 1) { uint8_t s = status; status = 0; return s; } uint8_t v = readbuf; readbuf = vram[vaddr]; vaddr = (vaddr + 1) & 16383; return v; }
  if (p >= 0xc0) { if (!(p & 1)) { uint8_t v = 0xff; if (keys & 64) v &= ~1; if (keys & 128) v &= ~2; if (keys & 32) v &= ~4; if (keys & 16) v &= ~8; if (keys & 1) v &= ~16; if (keys & 2) v &= ~32; return v; } return 0xff; }
  return 0xff;
}
static void port_wr(unsigned p, unsigned v) {
  p &= 255; port_out[p]++;
  if (p >= 0x80 && p < 0xc0) {
    if (p & 1) { if (!cmd_first) { cmd_lo = (uint8_t)v; cmd_first = 1; } else { cmd_first = 0; vcode = (uint8_t)(v >> 6); vaddr = (((v & 63) << 8) | cmd_lo) & 16383;
        if (vcode == 2) vr[v & 15] = cmd_lo; else if (!vcode) { readbuf = vram[vaddr]; vaddr = (vaddr + 1) & 16383; } } }
    else { cmd_first = 0; if (vcode == 3) cram[vaddr & (mode_gg ? 63 : 31)] = (uint8_t)v; else vram[vaddr] = (uint8_t)v; vaddr = (vaddr + 1) & 16383; readbuf = (uint8_t)v; }
  }
}
#define RD8(a) rd8(a)
#define WR8(a, v) wr8((a), (v))
#define PORT_IN(p) (cyc_cur = cyc, port_rd(p))
#define PORT_OUT(p, v) (cyc_cur = cyc, port_wr((p), (v)))
#define Z80F_STATIC static
#include "../core/z80f.h"

#ifdef WITH_PROBE   /* gcc -DWITH_PROBE -DPROBE_HOST -I. -Iinclude tools/sms_host.c idle_probe.c : checks the pure-loop probe (idle_probe.c) on real game states */
#include "../z80a.h"
uint8_t z80a_ctx[0x800]; int probe_dbg; static int hash_out;
uint32_t host_rd8(uint32_t a) { return rd8(a); }
int psc8_idle_probe(unsigned vdp_status, uint32_t avail, uint32_t *used);
static unsigned long probe_skips, probe_skipped_t, probe_calls, probe_bad; static unsigned probe_every; static int probe_check;
static void probe_ctx(Z80F *z) {
  memset(z80a_ctx, 0, sizeof z80a_ctx);
#define U32(o) (*(uint32_t *)(z80a_ctx + (o)))
  U32(Z80A_A) = z->a; U32(Z80A_F) = z->f; U32(Z80A_BC) = (z->b << 8) | z->c; U32(Z80A_DE) = (z->d << 8) | z->e; U32(Z80A_HL) = (z->h << 8) | z->l;
  U32(Z80A_PC) = z->pc; U32(Z80A_SP) = z->sp; U32(Z80A_IX) = z->ix; U32(Z80A_IY) = z->iy;
  U32(Z80A_AF2) = (z->a_ << 8) | z->f_; U32(Z80A_BC2) = (z->b_ << 8) | z->c_; U32(Z80A_DE2) = (z->d_ << 8) | z->e_; U32(Z80A_HL2) = (z->h_ << 8) | z->l_;
  z80a_ctx[Z80A_IFF1] = z->iff1; z80a_ctx[Z80A_IFF2] = z->iff2; z80a_ctx[Z80A_IM] = z->im; z80a_ctx[Z80A_HALTED] = z->halted; z80a_ctx[Z80A_INTP] = z->int_pending;
  unsigned save_in[256]; for (int i = 0; i < 256; i++) save_in[i] = (unsigned)port_in[i];
  unsigned sv = status, sc = cmd_first, sa = readbuf, sd = vaddr;
  for (unsigned p = 0; p < 0x100; p++) if (p < 0x40 || p >= 0xc0) { z80a_ctx[Z80A_FASTIN + p] = 1; z80a_ctx[Z80A_VALIN + p] = port_rd(p); }
  status = (uint8_t)sv; cmd_first = (uint8_t)sc; readbuf = (uint8_t)sa; vaddr = sd;                       /* port_rd side effects of the harness: undo */
  for (int i = 0; i < 256; i++) port_in[i] = save_in[i];
  z80a_ctx[Z80A_CMDFIRST] = cmd_first;
}
/* returns 0 nothing, 1 pure loop (not applied), 2 counter loop applied to *z (guest time advanced) */
static int probe_apply(Z80F *z, uint32_t avail, int apply) {
  probe_ctx(z); probe_calls++;
  Z80F zcopy = *z; Z80F *zsave = &zcopy; (void)zsave;
  uint32_t used = 0; int r = psc8_idle_probe(status, avail, &used);
  if (r == 2 && apply) {
    z->a = U32(Z80A_A); z->f = U32(Z80A_F); z->b = U32(Z80A_BC) >> 8; z->c = U32(Z80A_BC); z->d = U32(Z80A_DE) >> 8; z->e = U32(Z80A_DE); z->h = U32(Z80A_HL) >> 8; z->l = U32(Z80A_HL);
    z->pc = U32(Z80A_PC); z->sp = U32(Z80A_SP);
    z->a_ = U32(Z80A_AF2) >> 8; z->f_ = U32(Z80A_AF2); z->b_ = U32(Z80A_BC2) >> 8; z->c_ = U32(Z80A_BC2); z->d_ = U32(Z80A_DE2) >> 8; z->e_ = U32(Z80A_DE2); z->h_ = U32(Z80A_HL2) >> 8; z->l_ = U32(Z80A_HL2);
    { extern uint32_t psc8_probe_rinc; z->r = (uint8_t)(z->r + psc8_probe_rinc); }
    z->cyc += used; probe_skips++; probe_skipped_t += used;
    if (probe_check) {                                   /* replay the skipped time on a copy with the plain core and compare */
      Z80F zb = *zsave; unsigned save_in[256]; for (int i = 0; i < 256; i++) save_in[i] = (unsigned)port_in[i];
      while ((int32_t)(zb.cyc - z->cyc) < 0) z80f_run(&zb, zb.cyc + 1);
      for (int i = 0; i < 256; i++) port_in[i] = save_in[i];
      if (memcmp(&zb, z, sizeof zb)) { probe_bad++; if (probe_bad <= 6) fprintf(stderr, "MISMATCH after skip of %u T (from pc %04x): plain pc %04x a %02x f %02x bc %02x%02x de %02x%02x hl %02x%02x sp %04x cyc %u | skip pc %04x a %02x f %02x bc %02x%02x de %02x%02x hl %02x%02x sp %04x cyc %u\n", used, zsave->pc, zb.pc, zb.a, zb.f, zb.b, zb.c, zb.d, zb.e, zb.h, zb.l, zb.sp, zb.cyc, z->pc, z->a, z->f, z->b, z->c, z->d, z->e, z->h, z->l, z->sp, z->cyc); }
    }
  }
  return r;
}
static uint64_t fnv(uint64_t h, const void *p, size_t n) { const uint8_t *b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
static uint64_t state_hash(const Z80F *z) {
  uint64_t h = 1469598103934665603ull;
  Z80F zc = *z; h = fnv(h, &zc, sizeof zc); h = fnv(h, ram, sizeof ram); h = fnv(h, vram, sizeof vram); h = fnv(h, cram, sizeof cram); h = fnv(h, vr, sizeof vr); h = fnv(h, banks, sizeof banks);
  h = fnv(h, sram, 4096); h = fnv(h, &status, 1); h = fnv(h, &vaddr, sizeof vaddr); h = fnv(h, &cmd_first, 1); h = fnv(h, &readbuf, 1);
  return h;
}
#endif
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: sms_host ROM gg|sms FRAMES [pcs N] [script bench|none|monkey]\n"); return 2; }
  FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  fseek(f, 0, 2); long n = ftell(f); rewind(f); cart = malloc(n); if (fread(cart, 1, n, f) != (size_t)n) return 2; fclose(f);
  cart_size = (unsigned)n; cart_mask = cart_size - 1; mode_gg = !strcmp(argv[2], "gg");
  unsigned frames = (unsigned)atoi(argv[3]), npcs = 24, from = 0, seed = 1; int bench = 1, monkey = 0; double heavy_thr = 0.6;
  for (int i = 4; i + 1 < argc; i += 2) { if (!strcmp(argv[i], "pcs")) npcs = (unsigned)atoi(argv[i + 1]); else if (!strcmp(argv[i], "script")) { bench = !strcmp(argv[i + 1], "bench"); monkey = !strcmp(argv[i + 1], "monkey"); } else if (!strcmp(argv[i], "from")) from = (unsigned)atoi(argv[i + 1]); else if (!strcmp(argv[i], "seed")) seed = (unsigned)atoi(argv[i + 1]); else if (!strcmp(argv[i], "heavy")) heavy_thr = atof(argv[i + 1]); else if (!strcmp(argv[i], "ops")) op_dump = atoi(argv[i + 1]);
#ifdef WITH_PROBE
    else if (!strcmp(argv[i], "probedbg")) probe_dbg = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "probe")) probe_every = (unsigned)atoi(argv[i + 1]);          /* attempt (and apply) a counter-loop skip every N instructions */
    else if (!strcmp(argv[i], "hash")) hash_out = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "check")) probe_check = atoi(argv[i + 1]);
#endif
  }
  memset(sram, 0xff, sizeof sram);
  banks[1] = 0; banks[2] = 1; banks[3] = 2;
  Z80F z; z80f_reset(&z); uint32_t cyc_total = 0;
  unsigned long irq_frame = 0, instr = 0;
  for (unsigned fr = 0; fr < frames; fr++) {
    unsigned t = fr;
    keys = 0;
    if (monkey) { static unsigned mk, mt; if (!mt) { seed = seed * 1103515245u + 12345u; unsigned r = seed >> 16; mt = 4 + (r % 24); static const unsigned tab[] = { 0, 0, 64, 128, 32, 16, 1, 2, 64, 128, 32, 16, 1, 1, 2, 0 }; mk = tab[(r >> 5) % 16]; if (!(r % 23)) mk = 8; } mt--; keys = mk; }
    uint32_t busyT = 0; unsigned long fi = 0; memset(frame_hist, 0, sizeof frame_hist);
    if (bench) { if (t % 240 >= 120 && t % 240 < 124) keys = 8; else if (t % 240 >= 200 && t % 240 < 204) keys = 1; }
    line0 = z.cyc; uint32_t target = line0 + 59736u; unsigned line = 0, linecounter = vr[10]; scanline = 0;
    while ((int32_t)(z.cyc - target) < 0) {
      uint16_t pc = z.pc; uint32_t c0 = z.cyc;
      if (z.halted && !z.int_pending) { /* idle until the next line event */
        uint32_t nl = line0 + (line + 1u) * 228u; z.cyc = nl > c0 ? nl : c0 + 4; if (fr >= from) { cyc_hist[pc] += z.cyc - c0; cnt_hist[pc]++; }
      } else {
#ifdef WITH_PROBE
        if (probe_every && !z.halted && !z.int_pending && (instr % probe_every) == 0) {   /* same event bound as psc8.c's frame_cpu */
          unsigned ev = 262; if (line < 192) { ev = 192; if (vr[0] & 16) { unsigned l2 = line + linecounter + 1u; if (l2 < ev) ev = l2; } }
          int32_t av = (int32_t)(line0 + ev * 228u - z.cyc);
          if (av > 0 && probe_apply(&z, (uint32_t)av, 1) == 2) { pc = z.pc; c0 = z.cyc; }
        }
#endif
        if (!z.halted && z.int_pending && z.iff1) { /* single-stepping never reaches the core's own accept point */ z.iff1 = z.iff2 = 0; z.int_pending = 0; z.sp -= 2; wr8(z.sp, z.pc & 255); wr8(z.sp + 1u, z.pc >> 8); z.pc = z.im == 2 ? (rd8(((unsigned)z.i << 8) | 0xff) | (rd8((((unsigned)z.i << 8) | 0xff) + 1) << 8)) : 0x38; z.cyc += 13; pc = z.pc; c0 = z.cyc; } if (fr >= from && op_dump) { unsigned b0 = rd8(pc), b1 = rd8(pc + 1); if (b0 == 0xed) op_hist[1][b1]++; else if (b0 == 0xdd || b0 == 0xfd) { if (b1 == 0xcb) op_hist[4][rd8(pc + 3)]++; else op_hist[2][b1]++; } else if (b0 == 0xcb) op_hist[3][b1]++; else op_hist[0][b0]++; } z80f_run(&z, z.cyc + (z.halted ? 5 : 1)); if (fr >= from) { cyc_hist[pc] += z.cyc - c0; cnt_hist[pc]++; frame_hist[pc] += z.cyc - c0; busyT += z.cyc - c0; fi++; } instr++; }
      while (line < 262 && (int32_t)(z.cyc - (line0 + (line + 1u) * 228u)) >= 0) {
        scanline = ++line;
        if (line == 192) { status |= 128; if (vr[1] & 32) { z.int_pending = 1; z.int_data = 0xff; irq_frame++; } }
        if (line <= 192) { if (!linecounter--) { linecounter = vr[10]; if (vr[0] & 16) { z.int_pending = 1; z.int_data = 0xff; } } }
      }
    }
    cyc_total = z.cyc;
#ifdef WITH_PROBE
    if (hash_out) { printf("frame %u cyc %u hash %016llx\n", fr, z.cyc, (unsigned long long)state_hash(&z));
      if (hash_out > 1) { Z80F zc = z; zc.r = 0; uint64_t h0 = 1469598103934665603ull; printf("  parts z %016llx ram %016llx vram %016llx cram %016llx vr %016llx bank %016llx sram %016llx st %02x va %04x cf %d rb %02x | pc %04x sp %04x a %02x f %02x\n", (unsigned long long)fnv(h0, &zc, sizeof zc), (unsigned long long)fnv(h0, ram, sizeof ram), (unsigned long long)fnv(h0, vram, sizeof vram), (unsigned long long)fnv(h0, cram, sizeof cram), (unsigned long long)fnv(h0, vr, sizeof vr), (unsigned long long)fnv(h0, banks, sizeof banks), (unsigned long long)fnv(h0, sram, 4096), status, vaddr, cmd_first, readbuf, z.pc, z.sp, z.a, z.f); } }
#endif
    if (getenv("SMSDBG") && (fr % atoi(getenv("SMSDBG")) == 0)) fprintf(stderr, "fr %u pc %04x sp %04x iff1 %d im %d halted %d intp %d a %02x ram[0]=%02x c208=%02x vr1=%02x status %02x\n", fr, z.pc, z.sp, z.iff1, z.im, z.halted, z.int_pending, z.a, ram[0], ram[0x208], vr[1], status);
    if (fr >= from) { double fb = (double)busyT / 59736.0; unsigned bin = fb > 1 ? 10 : (unsigned)(fb * 10); busy_bins[bin]++; all_frames++; instr_all += fi;
      if (fb >= heavy_thr) { heavy_frames++; instr_busy += fi; for (int i = 0; i < 65536; i++) heavy_hist[i] += frame_hist[i]; } }
  }
  printf("frames %u instrs %lu cycles %u frame-irqs %lu\n", frames, instr, cyc_total, irq_frame);
#ifdef WITH_PROBE
  printf("probe: %lu mismatches; %lu calls, %lu counter-loop skips, %lu T-states skipped (%.1f%% of the run)\n", probe_bad, probe_calls, probe_skips, probe_skipped_t, 100.0 * probe_skipped_t / (cyc_total ? cyc_total : 1));
#endif
  printf("SRAM writes %lu max offset 0x%04lx\n", sram_wr, sram_max);
  printf("port IN:"); for (int p = 0; p < 256; p++) if (port_in[p] > 1000) printf(" %02x=%lu", p, port_in[p]); printf("\n");
  printf("port OUT:"); for (int p = 0; p < 256; p++) if (port_out[p] > 1000) printf(" %02x=%lu", p, port_out[p]); printf("\n");
  printf("Z80 busy fraction per frame (non-HALT T / 59736), %lu frames:", all_frames); for (int b = 0; b < 11; b++) printf(" %d0%%:%u", b, busy_bins[b]); printf("\n");
  printf("frames with busy >= %.0f%%: %lu (%.1f%%); mean instructions per frame %.0f, in heavy frames %.0f\n", heavy_thr * 100, heavy_frames, 100.0 * heavy_frames / (all_frames ? all_frames : 1), (double)instr_all / (all_frames ? all_frames : 1), (double)instr_busy / (heavy_frames ? heavy_frames : 1));
  if (heavy_frames) { printf("hottest PCs in heavy frames:\n"); uint64_t ht = 0; for (int i = 0; i < 65536; i++) ht += heavy_hist[i];
    for (unsigned k = 0; k < npcs; k++) { uint32_t best = 0; int bi = -1; for (int i = 0; i < 65536; i++) if (heavy_hist[i] > best) { best = heavy_hist[i]; bi = i; }
      if (bi < 0) break; printf("  %04x %6.2f%% %02x %02x %02x\n", bi, 100.0 * best / ht, rd8(bi), rd8(bi + 1), rd8(bi + 2)); heavy_hist[bi] = 0; } }
  uint64_t tot = 0; for (int i = 0; i < 65536; i++) tot += cyc_hist[i];
  for (unsigned k = 0; k < npcs; k++) {
    uint32_t best = 0; int bi = -1; for (int i = 0; i < 65536; i++) if (cyc_hist[i] > best) { best = cyc_hist[i]; bi = i; }
    if (bi < 0) break;
    printf("%04x %6.2f%% %10u T  x%u  %02x %02x %02x\n", bi, 100.0 * best / tot, best, cnt_hist[bi], rd8(bi), rd8(bi + 1), rd8(bi + 2)); cyc_hist[bi] = 0;
  }
  if (op_dump) { static const char *nm[5] = {"op", "ed", "xy", "cb", "xcb"}; for (int g = 0; g < 5; g++) for (int o = 0; o < 256; o++) if (op_hist[g][o]) printf("OPS %s_%02X %llu\n", nm[g], o, (unsigned long long)op_hist[g][o]); }
  return 0;
}
