// gbake: bake the wavetable timbre snapshots of the Genesis adventures' FM patches (host tool, not part of the ROM).
//
//   gbake zones.txt bank_raw.bin [-stages 8] [-kcap 40] [-v]
//
// Every zone is one sub-voice (a carrier together with the modulator chain that feeds it, see SUBV; tools/zones.py
// lists them) played in one octave block.  It is rendered with the carrier envelope held flat (the runtime applies
// the real carrier envelope) and the modulators keeping their real envelopes.  At a few times after key-on the output
// is analysed over whole periods of the fundamental, relative to the key-on phase, and re-synthesised as a band-limited,
// periodic table.  The render is done with ymfm (BSD-3-Clause, A. Giles) - only here, offline; the ROM carries just
// the snapshots, exactly as the PSIV bank does.
//
// bank_raw.bin: repeated records  u32 hash, u8 block, u8 sub, u8 rho, u8 0, u8 gain[STAGES_MAX], u8 nbits[STAGES_MAX], u8 noise[STAGES_MAX], then the
// stage tables (2^nbits[s] int8 each).  A stage keeps only the harmonics that rise above -thr dB of its own peak, so late
// (dull) stages get short tables.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include "ymfm_opn.h"
extern "C" {
#include <stdint.h>
}
#include "../gfm_tabs.h"

static const int STAGES_MAX = 8;
static const double YM_RATE = 7670453.6 / 144.0;      // YM2612 sample rate
static const double OUT_RATE = 13379.0;               // GBA mixer rate
static const double STAGE_MS[STAGES_MAX] = {6, 20, 50, 110, 240, 520, 1100, 2400};
static const int SLOT_OF_OP[4] = {0, 2, 1, 3};         // operator index -> register slot

struct SubV { int car, mask; };
static const std::vector<SubV> SUBV[8] = {
  {{3, 15}}, {{3, 15}}, {{3, 15}}, {{3, 15}},
  {{1, 3}, {3, 12}},
  {{1, 3}, {2, 5}, {3, 9}},
  {{1, 3}, {2, 4}, {3, 8}},
  {{0, 1}, {1, 2}, {2, 4}, {3, 8}},
};

struct Zone { uint32_t hash; int alg, fb, block, fnum, fmax, count, vi; uint8_t op[4][6]; };

static uint32_t mul2_of(uint8_t dtmul) { uint32_t m = (dtmul & 15) * 2u; return m ? m : 1u; }

static uint32_t pstep_of(int block, int fnum, uint8_t dtmul) {       // ymfm phase_step per YM sample (10.10), as the chip computes it
  uint32_t bf = ((uint32_t)block << 11) | (uint32_t)fnum;
  uint32_t keycode = (((bf >> 10) & 0xf) << 1) | ((0xfe80u >> ((bf >> 7) & 0xf)) & 1);
  uint32_t fn2 = ((uint32_t)fnum & 0x7ff) << 1;
  uint32_t base = (fn2 << block) >> 2;
  uint32_t dt = (dtmul >> 4) & 7;
  int32_t det = gfm_detune[keycode * 4 + (dt & 3)];
  if (dt & 4) det = -det;
  uint32_t ps = (base + (uint32_t)det) & 0x1ffff;
  return (ps * mul2_of(dtmul)) >> 1;
}

static uint32_t gcd(uint32_t a, uint32_t b) { while (b) { uint32_t t = a % b; a = b; b = t; } return a; }

// energy (mean square) of x[n0, n0+nn) below the mixer's Nyquist (nn a power of two)
static void fft(std::vector<double> &re, std::vector<double> &im) {
  size_t n = re.size();
  for (size_t i = 1, j = 0; i < n; i++) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
  for (size_t len = 2; len <= n; len <<= 1) {
    double ang = -2.0 * M_PI / (double)len;
    for (size_t i = 0; i < n; i += len) for (size_t k = 0; k < len / 2; k++) {
      double wr = cos(ang * k), wi = sin(ang * k);
      double ur = re[i + k], ui = im[i + k], vr = re[i + k + len / 2] * wr - im[i + k + len / 2] * wi, vi = re[i + k + len / 2] * wi + im[i + k + len / 2] * wr;
      re[i + k] = ur + vr; im[i + k] = ui + vi; re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
    }
  }
}
// the render, low-passed at the mixer's Nyquist (brick wall): what the mixer could ever play of it
static std::vector<double> lowpass(const std::vector<double> &x) {
  int np = 1; while (np < (int)x.size()) np <<= 1;
  std::vector<double> re(np, 0.0), im(np, 0.0);
  for (size_t n = 0; n < x.size(); n++) re[n] = x[n];
  fft(re, im);
  int kb = (int)(np * OUT_RATE / 2.0 / YM_RATE);
  for (int k = 0; k < np; k++) { int kk = k <= np / 2 ? k : np - k; if (kk > kb) re[k] = im[k] = 0; }
  for (int k = 0; k < np; k++) im[k] = -im[k];                 // inverse transform through the forward one
  fft(re, im);
  std::vector<double> y(x.size());
  for (size_t n = 0; n < x.size(); n++) y[n] = re[n] / np;
  return y;
}
static double ms_of(const std::vector<double> &x, int n0, int nn) {
  double e = 0; for (int n = 0; n < nn; n++) { double v = x[(size_t)(n0 + n)]; e += v * v; }
  return e / nn;
}

static std::vector<double> render(const Zone &z, const SubV &sv, int nsamp) {
  ymfm::ymfm_interface intf; ymfm::ym3438 chip(intf); chip.reset();
  auto wr = [&](int reg, int val) { chip.write(0, reg); chip.write(1, val); };
  static const int CARR[8] = {8, 8, 8, 8, 10, 14, 14, 15};
  for (int i = 0; i < 4; i++) {
    int off = SLOT_OF_OP[i];                                          // channel 0
    uint8_t r[6]; memcpy(r, z.op[i], 6);
    bool in = (sv.mask >> i) & 1;
    if (!in) r[1] = 127;                                              // not part of this sub-voice: silent
    if (i == sv.car) {                                                // carrier: full level, flat envelope (runtime supplies the real one)
      r[1] = 0; r[2] = (uint8_t)((r[2] & 0xc0) | 0x1f); r[3] = 0; r[4] = 0; r[5] = 0x0f;
    }
    (void)CARR;
    wr(0x30 + 4 * off, r[0]); wr(0x40 + 4 * off, r[1]); wr(0x50 + 4 * off, r[2]);
    wr(0x60 + 4 * off, r[3]); wr(0x70 + 4 * off, r[4]); wr(0x80 + 4 * off, r[5]);
  }
  wr(0xb0, z.alg | (z.fb << 3)); wr(0xb4, 0xc0);
  wr(0xa4, (z.block << 3) | (z.fnum >> 8)); wr(0xa0, z.fnum & 255);
  wr(0x28, 0xf0);
  std::vector<double> x(nsamp);
  for (int n = 0; n < nsamp; n++) { ymfm::ymfm_output<2> o; chip.generate(&o, 1); x[n] = o.data[0] * 6.0 / 128.0; }
  return x;
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: gbake zones.txt bank_raw.bin [-stages N] [-kcap K] [-v]\n"); return 2; }
  int stages = 8, kcap = 40, sigma = 0, fmaxmode = 1, nmax = 256, nmin = 32; double nk = 8.0, thr_db = -48.0; int minuse = 0, minpatch = 0, nmaxlow = 0, lowblk = 2, nzmode = 1; double nzscale = 1.0, nzmin = 0.0; bool verbose = false;
  for (int i = 3; i < argc; i++) {
    if (!strcmp(argv[i], "-stages")) stages = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-kcap")) kcap = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-v")) verbose = true;
    else if (!strcmp(argv[i], "-sigma")) sigma = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-fmax")) fmaxmode = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-nmax")) nmax = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-nk")) nk = atof(argv[++i]);
    else if (!strcmp(argv[i], "-nmin")) nmin = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-thr")) thr_db = atof(argv[++i]);
    else if (!strcmp(argv[i], "-minuse")) minuse = atoi(argv[++i]);        // drop a zone used fewer times than this if its patch has a more used zone
    else if (!strcmp(argv[i], "-minpatch")) minpatch = atoi(argv[++i]);    // drop whole patches used fewer times than this
    else if (!strcmp(argv[i], "-nmaxlow")) nmaxlow = atoi(argv[++i]);      // table size limit for the low octaves (blocks <= -lowblk)
    else if (!strcmp(argv[i], "-lowblk")) lowblk = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-nz")) nzmode = atoi(argv[++i]);              // 0: no noise supplement
    else if (!strcmp(argv[i], "-nzscale")) nzscale = atof(argv[++i]);        // scale of the supplement level
    else if (!strcmp(argv[i], "-nzmin")) nzmin = atof(argv[++i]);            // only where the lost energy fraction exceeds this
  }
  std::vector<Zone> zones;
  { FILE *f = fopen(argv[1], "r"); char line[512];
    while (fgets(line, sizeof line, f)) {
      Zone z; unsigned h; int n = 0;
      if (sscanf(line, "%x %d %d %d %d%n", &h, &z.alg, &z.fb, &z.block, &z.fnum, &n) < 5) continue;
      z.hash = h; const char *p = line + n;
      for (int i = 0; i < 4; i++) for (int k = 0; k < 6; k++) { unsigned v; int m = 0; sscanf(p, "%x%n", &v, &m); p += m; z.op[i][k] = (uint8_t)v; }
      { int fm = 0, ct = 1000, vi = 0; if (sscanf(p, "%d %d %d", &fm, &ct, &vi) < 3) { fprintf(stderr, "bad zone line\n"); return 2; } z.fmax = std::max(fm, z.fnum); z.count = ct; z.vi = vi; }
      zones.push_back(z);
    } fclose(f); }
  { std::vector<Zone> keep;
    for (const Zone &z : zones) {
      int tot = 0, best = 0; for (const Zone &w : zones) if (w.hash == z.hash) { tot += w.count; best = std::max(best, w.count); }
      if (tot < minpatch) continue;
      if (z.count < minuse && best >= minuse) continue;
      keep.push_back(z);
    }
    fprintf(stderr, "zones %zu -> %zu\n", zones.size(), keep.size()); zones = keep; }
  FILE *out = fopen(argv[2], "wb");
  size_t total = 0; int nrec = 0;
  const int NS = (int)(STAGE_MS[stages - 1] * 1e-3 * YM_RATE) + 12000;
  for (const Zone &z : zones) {
    const std::vector<SubV> &subs = SUBV[z.alg];
    for (size_t si = (size_t)z.vi; si == (size_t)z.vi; si++) {              // the sub-voice this zone line stands for
      const SubV &sv = subs[si];
      bool table = (sv.mask & ~(1 << sv.car)) || (sv.car == 0 && z.fb);
      if (!table) continue;                                           // single plain operator: the runtime plays a sine
      // fundamental: gcd of the multiples of the operators of this sub-voice
      uint32_t g = 0;
      for (int i = 0; i < 4; i++) if ((sv.mask >> i) & 1) g = gcd(g, mul2_of(z.op[i][0]));
      uint32_t rho = mul2_of(z.op[sv.car][0]) / g;
      double ps = pstep_of(z.block, z.fnum, z.op[sv.car][0]);
      double P = 1048576.0 / ps * rho;                                 // YM samples per fundamental cycle
      // table size / harmonics: highest fundamental in this block decides how many harmonics fit under the mixer's Nyquist
      double ps_hi = pstep_of(z.block, fmaxmode ? z.fmax : 2047, z.op[sv.car][0]);
      double f0_hi = ps_hi / 1048576.0 * YM_RATE / rho;
      int kmax = (int)floor(OUT_RATE / 2.0 / f0_hi);
      if (kmax > kcap) kmax = kcap;
      if (kmax < 1) kmax = 1;
      std::vector<double> x = render(z, sv, NS), xlp = lowpass(x);
      // an operator of the sub-voice that runs above ~24 kHz (nearly the chip's own Nyquist) makes the chip alias: the sound is a noise-like
      // buzz that no periodic table can follow.  The runtime plays band-limited noise at the measured level instead (rho 0, no tables).
      double fop_hi = 0;
      for (int i = 0; i < 4; i++) if ((sv.mask >> i) & 1) fop_hi = std::max(fop_hi, pstep_of(z.block, z.fmax, z.op[i][0]) / 1048576.0 * YM_RATE);
      if (fop_hi >= 0.45 * YM_RATE) {
        uint8_t ngain[STAGES_MAX] = {0}, nnb[STAGES_MAX]; std::vector<int8_t> zt(1 << 5, 0);
        for (int s = 0; s < stages; s++) {
          int nn = 4000; double t0 = STAGE_MS[s] * 1e-3 * YM_RATE; int n0 = (int)std::max(0.0, std::floor(t0 - nn / 2));
          double rms = sqrt(ms_of(xlp, n0, nn));
          double g8 = 256.0 * rms / (255.25 * 1.25); if (g8 > 255) g8 = 255;
          ngain[s] = rms < 0.1 ? 0 : (uint8_t)lround(g8); nnb[s] = 5;
        }
        uint8_t hdr[8 + 3 * STAGES_MAX]; memcpy(hdr, &z.hash, 4); hdr[4] = (uint8_t)z.block; hdr[5] = (uint8_t)si; hdr[6] = 0; hdr[7] = 0;
        memcpy(hdr + 8, ngain, STAGES_MAX); memcpy(hdr + 8 + STAGES_MAX, nnb, STAGES_MAX); memset(hdr + 8 + 2 * STAGES_MAX, 0, STAGES_MAX);
        fwrite(hdr, 1, sizeof hdr, out); total += sizeof hdr;
        for (int s = 0; s < stages; s++) { fwrite(zt.data(), 1, zt.size(), out); total += zt.size(); }
        nrec++;
        printf("zone %08x alg %d fb %d blk %d fnum %4d sub %zu car %d: NOISE  gains %d %d %d %d %d %d %d %d\n", z.hash, z.alg, z.fb, z.block, z.fnum, si, sv.car, ngain[0], ngain[1], ngain[2], ngain[3], ngain[4], ngain[5], ngain[6], ngain[7]);
        continue;
      }
      std::vector<int8_t> tabs[STAGES_MAX]; uint8_t gain[STAGES_MAX] = {0}, nbs[STAGES_MAX] = {0};
      int kfull = kmax; double lost[STAGES_MAX] = {0}; uint8_t nzg[STAGES_MAX] = {0};
      for (int s = 0; s < stages; s++) {
        double W = std::max(2048.0, 3.0 * P);
        double L = std::ceil(W / P); W = L * P;
        double t0 = STAGE_MS[s] * 1e-3 * YM_RATE;
        double n0 = std::max(0.0, std::floor(t0 - W / 2));
        int nn = (int)floor(W);
        std::vector<double> A(kfull + 1), B(kfull + 1);
        for (int k = 1; k <= kfull; k++) {
          double a = 0, b = 0, w = 2.0 * M_PI * k / P;
          for (int n = 0; n < nn; n++) { double t = n0 + n; double xv = x[(size_t)t]; a += xv * cos(w * t); b += xv * sin(w * t); }
          A[k] = 2.0 * a / nn; B[k] = 2.0 * b / nn;
        }
        double Et = ms_of(xlp, (int)n0, nn);
        // keep the harmonics that matter: those above thr_db of the stage's own peak (the sum of all harmonics is an upper bound of it)
        double tot = 0; for (int k = 1; k <= kfull; k++) tot += hypot(A[k], B[k]);
        double thr = tot * pow(10.0, thr_db / 20.0);
        int ks = 1; for (int k = 1; k <= kfull; k++) if (hypot(A[k], B[k]) >= thr) ks = k;
        int nlim = (nmaxlow && z.block <= lowblk) ? nmaxlow : nmax;
        int N = nmin; while (N < nk * ks && N < nlim) N <<= 1;
        if (ks > N / nk) ks = (int)(N / nk);
        int nb = 0; while ((1 << nb) < N) nb++;
        // what the table does not carry (harmonics above its limit, chip aliasing that is no harmonic at all) is played as noise at its level
        { double Ek = 0; for (int k = 1; k <= ks; k++) Ek += 0.5 * (A[k] * A[k] + B[k] * B[k]);
          double lm = std::max(0.0, Et - Ek); lost[s] = Et > 1e-3 ? lm / Et : 0.0;
          double gn = (nzmode && lost[s] >= nzmin) ? nzscale * 256.0 * sqrt(lm) / (255.25 * 1.25) : 0.0; if (gn > 255) gn = 255; nzg[s] = gn < 1.5 ? 0 : (uint8_t)lround(gn); }
        std::vector<double> y(N, 0.0);
        for (int k = 1; k <= ks; k++) {
          double sg = sigma ? sin(M_PI * k / (ks + 1)) / (M_PI * k / (ks + 1)) : 1.0;   // Lanczos sigma (optional)
          for (int j = 0; j < N; j++) { double th = 2.0 * M_PI * k * j / N; y[j] += sg * (A[k] * cos(th) + B[k] * sin(th)); }
        }
        // table phase: harmonic k is cos(2 pi k t / P) = cos(2 pi k * (phase of the fundamental)), phase 0 at key-on
        double pk = 0; for (int j = 0; j < N; j++) pk = std::max(pk, fabs(y[j]));
        double g8 = 256.0 * pk / (255.25 * 1.25); if (g8 > 255) g8 = 255;   // 1.25 headroom: the band-limited waveform can overshoot a full carrier
        gain[s] = pk < 0.25 ? 0 : (uint8_t)lround(g8);
        nbs[s] = (uint8_t)nb;
        tabs[s].resize(N);
        for (int j = 0; j < N; j++) tabs[s][j] = (int8_t)(pk < 0.25 ? 0 : lround(127.0 * y[j] / pk));
        if (verbose) printf("  hash %08x blk %d sub %zu stage %d  K=%d/%d N=%d rho=%u peak %.1f\n", z.hash, z.block, si, s, ks, kfull, N, rho, pk);
      }
      uint8_t hdr[8 + 3 * STAGES_MAX]; memcpy(hdr, &z.hash, 4); hdr[4] = (uint8_t)z.block; hdr[5] = (uint8_t)si; hdr[6] = (uint8_t)rho; hdr[7] = 0;
      memcpy(hdr + 8, gain, STAGES_MAX); memcpy(hdr + 8 + STAGES_MAX, nbs, STAGES_MAX); memcpy(hdr + 8 + 2 * STAGES_MAX, nzg, STAGES_MAX);
      fwrite(hdr, 1, sizeof hdr, out); total += sizeof hdr;
      for (int s = 0; s < stages; s++) { fwrite(tabs[s].data(), 1, tabs[s].size(), out); total += tabs[s].size(); }
      nrec++;
      printf("lost %.2f %.2f %.2f %.2f nz %d %d %d %d\n", lost[1], lost[3], lost[5], lost[7], nzg[1], nzg[3], nzg[5], nzg[7]);
      printf("zone %08x alg %d fb %d blk %d fnum %4d sub %zu car %d: rho %u P=%.1f K=%d  N %d %d %d %d %d %d %d %d  gains %d %d %d %d %d %d %d %d\n", z.hash, z.alg, z.fb, z.block, z.fnum, si, sv.car,
             rho, P, kfull, 1 << nbs[0], 1 << nbs[1], 1 << nbs[2], 1 << nbs[3], 1 << nbs[4], 1 << nbs[5], 1 << nbs[6], 1 << nbs[7], gain[0], gain[1], gain[2], gain[3], gain[4], gain[5], gain[6], gain[7]);
    }
  }
  fclose(out);
  printf("records %d, %zu bytes\n", nrec, total);
  return 0;
}
