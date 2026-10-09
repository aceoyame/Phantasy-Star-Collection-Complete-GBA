// ymcmp: run a gsim YM2612 trace through ymfm and through gfm, compare the output-sample points.
//   ymcmp LOG [-n frames] [-mode ref|prod] [-v]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include "ymfm_opn.h"
extern "C" {
#include "../gfm.h"
}
struct Ev { uint64_t b; int part, reg, val; };
static std::vector<Ev> events;
int main(int argc, char **argv) {
  const char *log = argv[1]; int maxframes = 1 << 30; bool verbose = false; int chunk = 296; int startg = 0;
  for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "-n")) maxframes = atoi(argv[++i]); else if (!strcmp(argv[i], "-v")) verbose = true; else if (!strcmp(argv[i], "-chunk")) chunk = atoi(argv[++i]); }
  FILE *f = fopen(log, "r"); char line[256]; uint64_t lastb = 0;
  while (fgets(line, sizeof line, f)) {
    unsigned fr, t, part, reg, val;
    if (sscanf(line, "F%u T%u YM%u %x %x", &fr, &t, &part, &reg, &val) == 5) {
      if ((int)fr >= maxframes) break;
      uint64_t abs = (uint64_t)fr * 59659u + t;
      uint64_t sw = abs * 15 / 1008;
      uint64_t b = sw / GFM_DECIM;
      if (b < lastb) b = lastb;
      lastb = b; events.push_back({b, (int)part, (int)reg, (int)val});
    }
  }
  fclose(f);
  uint64_t total = (uint64_t)maxframes < (1u << 29) ? (uint64_t)maxframes * 59659u * 15 / 1008 / GFM_DECIM : lastb + 300;
  if ((uint64_t)maxframes >= (1u << 29)) total = lastb + 300;
  ymfm::ymfm_interface intf; ymfm::ym3438 chip(intf); chip.reset();
  gfm_init();
  std::vector<int32_t> aM(chunk), aL(chunk), aR(chunk);
  std::vector<int> yL(chunk), yR(chunk);
  size_t ei = 0; uint64_t bad = 0, cmp = 0; int maxd = 0; double sd = 0, sy = 0;
  for (uint64_t g0 = 0; g0 < total; g0 += chunk) {
    unsigned n = (unsigned)std::min<uint64_t>(chunk, total - g0);
    gfm_begin(aM.data(), aL.data(), aR.data(), n);
    size_t ej = ei;
    for (unsigned k = 0; k < n; k++) {
      uint64_t g = g0 + k;                                  // group index (0-based): events with b <= g apply before it
      while (ej < events.size() && events[ej].b <= g) {
        Ev &e = events[ej++];
        chip.write(e.part * 2, e.reg); chip.write(e.part * 2 + 1, e.val);
        gfm_write(e.part, e.reg, e.val, (unsigned)(std::max<uint64_t>(e.b, g0) - g0));
      }
      ymfm::ymfm_output<2> o[GFM_DECIM];
      for (int s = 0; s < GFM_DECIM; s++) chip.generate(&o[s], 1);
      int L = o[GFM_DECIM - 1].data[0], R = o[GFM_DECIM - 1].data[1];
      yL[k] = (int)lround(L * 6.0 / 128.0); yR[k] = (int)lround(R * 6.0 / 128.0);
    }
    ei = ej;
    gfm_end();
    for (unsigned k = 0; k < n; k++) {
      int gl = (aL[k] + aM[k]) / 8, gr = (aR[k] + aM[k]) / 8;
      int dl = gl - yL[k], dr = gr - yR[k];
      cmp += 2; if (dl) bad++; if (dr) bad++;
      if (abs(dl) > maxd) maxd = abs(dl); if (abs(dr) > maxd) maxd = abs(dr);
      sd += (double)dl * dl + (double)dr * dr; sy += (double)yL[k] * yL[k] + (double)yR[k] * yR[k];
      if (verbose && (dl || dr) && bad < 40) printf("g=%llu L %d vs %d  R %d vs %d\n", (unsigned long long)(g0 + k), gl, yL[k], gr, yR[k]);
    }
  }
  printf("samples %llu  mismatches %llu  max|diff| %d  snr %.1f dB  (ymfm rms %.1f)\n", (unsigned long long)cmp, (unsigned long long)bad, maxd,
         sd > 0 ? 10 * log10(sy / sd) : 999.0, sqrt(sy / cmp));
  return 0;
}
