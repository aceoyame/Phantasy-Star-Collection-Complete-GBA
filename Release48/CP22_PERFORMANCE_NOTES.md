# R48 CP22 — Native speed checkpoint (2026-10-08)

Run `RUN_THIS_R48_CP22_NATIVE_SPEED.gba`. It is the complete 32 MiB collection.
CRC32: `BE2820E0`.

## Actual status

Gaiden reaches full speed in the fixed guest-input windows below. Its longer
12,000-display-frame input run averages **99.02%**, with 1,000-frame windows
from **98.0% to 100%**. It is not certified full speed throughout the game.
PS1 still has slower windows. Game Gear Adventure reaches 100% in the later
fixed windows, but its opening and other scenes can dip. The Genesis
adventures remain substantially below full speed in busy gameplay.

These are mGBA 0.10.5 guest/display frame measurements, not hardware or whole-game
playthrough guarantees. The guest timing, samples and rendering cadence are
preserved. Fixed-window rates above 100% from producer-buffer lead are capped
at 100% in this table; raw results retain the original values.

## Changes from CP21

- Gaiden's byte-verified drawing loops execute native ARM spans and repeated
  iterations within the original event budget. Both coordinate SRL triplets
  have native spans. Short budgets, changed ROM mappings and mapper-register
  destinations fall back to the regular Z80 instructions.
- The late scanline-wait loop skips only complete 30-T-state iterations before
  both the threshold and event boundary. The final ordinary IN/CP/JR preserves
  CPU state and the V-counter discontinuity.
- PS1 and both Game Gear titles update only the cartridge window changed by a
  bank switch, including bank-1's mirrored pages. This removes redundant native
  address-map work; boot/reset still initializes the complete map.
- Game Gear's unused FM accumulator is not cleared. Its PSG PCM stays exact.
- A shared Genesis graphics loop and ADD.B leaf call are compiled as a native
  span. A full byte expands eight pixels using a 16-entry table; shorter spans
  retain the original iterations. RAM, stack writes, CPU flags, cycles, bus
  counts and RAM-code generation counts are preserved.
- Eighteen identical Genesis translations share code. Function-table indices
  stay unchanged; the additional-code region frees 7,060 bytes before PSC8 growth.

PSIV's useful approach here is to compile whole operations and update only
changed state. Its native game implementation avoids the two original CPUs and
VDP workload still needed by the adventures; adopting the same audio wave data
alone does not remove that CPU and graphics work.

## Same guest work, same inputs

| Game | Guest frames | CP21 speed | CP22 speed | Mean processing cycles/frame |
|---|---:|---:|---:|---:|
| PS1 | 100–400 | 95.85% | 95.85% | 193888 → 191692 |
| PS1 | 400–700 | 96.77% | 97.09% | 203444 → 200218 |
| PS1 | 700–1000 | 99.01% | 99.01% | 192683 → 186168 |
| Adventure | 100–400 | 99.01% | 99.01% | 116352 → 113543 |
| Adventure | 400–700 | 100.00% | 100.00% | 122625 → 119709 |
| Adventure | 700–1000 | 100.00% | 100.00% | 121545 → 118627 |
| Gaiden | 100–400 | 98.45% | 100.00% | 141910 → 122017 |
| Gaiden | 400–700 | 100.00% | 100.00% | 122448 → 119752 |
| Gaiden | 700–1000 | 100.00% | 100.00% | 113220 → 110717 |

PS1 processing work falls about 1.1–3.4%; Game Gear Adventure about 2.4%;
Gaiden's busiest sampled window about 14.0%. Lower processing work need not
change every display-rate window because buffer waits and scene bursts also
contribute.

| Genesis game | Guest frames | CP21 speed | CP22 speed |
|---|---:|---:|---:|
| Anne | 150–450 | 31.21% | 31.54% |
| Anne | 450–750 | 25.16% | 25.16% |
| Anne | 750–1050 | 32.85% | 32.85% |
| Nei | 150–450 | 39.28% | 39.80% |
| Nei | 450–750 | 36.24% | 36.24% |
| Nei | 750–1050 | 36.74% | 36.74% |

The first busy window gains about 1.06% for Anne and 1.33% for Nei; later
windows are effectively unchanged. The original 600-display-frame song-0x81
benchmark remains approximately 83–97%, essentially unchanged from CP21.
These title/music and busy-gameplay tests are different workloads. CP21's
450–700 window also differs from this checkpoint's 450–750 window; use the
CP21 baselines rerun here for comparisons.

## Validation

- All eight Genesis adventures: complete CPU, bus, EEPROM, RAM, Z80 RAM,
  VRAM, CRAM, VSRAM and RAM-code generation state exact over 1,050 scripted
  guest frames against CP21. Optimization-only spin/branch history is excluded.
- PS1, Adventure and Gaiden: 1,200 same-input guest frames each, CPU/memory,
  produced PCM and periodic GBA graphics/save bytes exact against CP21.
- Four synthetic VDP/IRQ/audio/stereo probes: 120 guest frames each exact.
- 574 Gaiden native-span boundary cases exact, including mapper destinations,
  short budgets, wraparound, disabled specialization and mapped-page bytes.
- 1,632 Genesis decoder boundary cases exact, including odd destinations,
  RAM-code generation boundaries, both entries and stack overlap fallback.
- Actual ARM/Thumb Genesis PCM: 500 sound frames each, Anne song 0x81 and
  Nei song 0x84, byte-exact against CP21.
- PS1/Gaiden SRAM mapped writes and power-cycle reload exact; all eight
  Genesis EEPROM slots reload a nondefault save word with other areas protected.
- General launch/menu/audio/PSII-debug and cartridge-preservation checks pass.
- Launcher 0x000000–0x048000, Genesis FM bank 0x048000–0x060000 and
  PSII/III/IV 0x080000–0x6CA500 match CP21 byte-for-byte.
- Final rebuild matches every byte of the tested ROM.

Full results are in `validation_cp22/`. ROM SHA256 is in `SHA256.txt`.

## Space and rebuild

ROM remains 33,554,432 bytes. PSC8 IWRAM ends at 0x03007734 (588 bytes below
its limit). Genesis IWRAM ends at 0x0300797C (only four bytes below its limit).
The additional translation region has 5,552 bytes free. Do not add Genesis
IWRAM code without moving or shrinking something else.

This ZIP contains complete source and immutable assets; no checkpoint overlay
chain is needed. `tools/build_cp22.sh /absolute/path/to/Release48` restores the
bundled LLVM18 Linux toolchain if needed and enforces the final CRC32. It calls
`source/build_cp22.py`. Keep the accepted generated Genesis translations:
older AOT regeneration can overwrite these optimizations.

For host verification, build mGBA with
`python3 source/tools_host/r46_mgba/build_minimal.py --out /absolute/runtime/path`
and the bridge with `source/tools_host/cp08_mgba/build_probe2.py --runtime-dir`
that path. Set PATH/LD_LIBRARY_PATH as in the build script. CP22 test programs
are `source/tools_host/check_cp22_*.py`; retain the CP21 reference ELF/ROM/source.

## Next work

Prioritize actual expensive gameplay frames: graphics composition and further
whole-routine CPU compilation. The event-driven Z80 sound replacement remains
research, not active runtime playback. Continue preserving full CPU/RAM/timer
correctness, PCM, SRAM, all titles and the 32 MiB limit. Do not reduce audio,
change guest timing or add frame dropping to obtain a benchmark number.
