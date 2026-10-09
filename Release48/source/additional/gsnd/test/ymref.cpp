// ymref: reference render of a sndtest event log with ymfm (offline QA only): YM2612 at its own rate -> raw float32 stereo.
//   ymref LOG OUT.f32 FRAMES     (event times are mapped onto the runtime's frame timeline: 224 samples at 13,379 Hz per frame)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "ymfm_opn.h"
struct Ev { uint64_t s; int part, reg, val; };
int main(int argc, char **argv) {
  const double YM_RATE = 7670453.6 / 144.0, FRAME_S = 224.0 / 13379.0; int frames = atoi(argv[3]);
  std::vector<Ev> ev; FILE *f = fopen(argv[1], "r"); char line[256];
  while (fgets(line, sizeof line, f)) {
    unsigned fr, t, part, reg, val;
    if (sscanf(line, "F%u T%u YM%u %x %x", &fr, &t, &part, &reg, &val) == 5) {
      double sec = ((double)fr + t / 59659.0) * FRAME_S;
      ev.push_back({(uint64_t)(sec * YM_RATE), (int)part, (int)reg, (int)val});
    }
  }
  fclose(f);
  ymfm::ymfm_interface intf; ymfm::ym3438 chip(intf); chip.reset();
  uint64_t total = (uint64_t)(frames * FRAME_S * YM_RATE); size_t ei = 0;
  FILE *o = fopen(argv[2], "wb");
  std::vector<float> buf; buf.reserve(1 << 16);
  for (uint64_t n = 0; n < total; n++) {
    while (ei < ev.size() && ev[ei].s <= n) { Ev &e = ev[ei++]; chip.write(e.part * 2, e.reg); chip.write(e.part * 2 + 1, e.val); }
    ymfm::ymfm_output<2> out; chip.generate(&out, 1);
    buf.push_back((float)(out.data[0] * 6.0 / 128.0)); buf.push_back((float)(out.data[1] * 6.0 / 128.0));
    if (buf.size() >= (1 << 16)) { fwrite(buf.data(), 4, buf.size(), o); buf.clear(); }
  }
  fwrite(buf.data(), 4, buf.size(), o); fclose(o);
  return 0;
}
