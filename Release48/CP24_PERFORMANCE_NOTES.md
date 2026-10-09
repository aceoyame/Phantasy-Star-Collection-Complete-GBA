# R48 CP24 — Adventure speed checkpoint (2026-10-08)

Run `Release48/binaries/RUN_THIS_R48_CP24_ADVENTURE_SPEED.gba`.
The complete collection remains 32 MiB. CRC32: `F35DA10F`.

## Changes

This combines our CP22 native Gaiden/mapper/Genesis decoder work with the
separate CP23 adventure divide-by-18 and exact ARM PCM saturation changes.
CP23's differently based CP22 overlay was not applied wholesale.

New adventure work:
- An aligned 320-pixel single plane expands each eight-pixel tile row into
  the original six sampled output pixels, written as three native pairs.
  Window text uses the same path. Transparent pixels keep the backdrop.
- Incremental pattern scans inspect only visible plane columns when horizontal
  scrolling is full-screen. Wrapped columns, other scroll modes, window,
  sprites, flips and priorities retain the original pixel semantics.
- Four MOVE.B/DBRA copy/fill loops run native ARM spans in ordinary RAM.
  Only complete pairs before the guest event limit are batched. Short
  budgets, A7 byte stepping, unsupported addresses and RAM wrapping fall back.
  Forward overlap, flags, counters and every RAM-code generation are exact.
- An exact divide-by-12 reciprocal removes software division from the fill
  spans. The divide-by-18 and CP22 divide-by-20 paths remain in place.

No audio downgrade or timing/cadence change was used. A precomposed row cache
was rejected because it slowed busy scenes. The delivered CP24 binary uses Thumb dirty collection; its earlier trial
summary describing an accepted ARM version was inaccurate.

## Same guest work and inputs

Baseline is the combined CP22/CP23 build, before this graphics and RAM-span
work. Both builds ran in unmodified mGBA 0.10.5 with the same guest inputs and
song request. Speed means guest frames divided by equivalent GBA display
frames, calculated from the native GBA cycle clock. These are scripted scene
windows, not whole-game or physical-hardware guarantees.

| Adventure | Guest frames | Merged baseline | CP24 | Relative gain |
|---|---:|---:|---:|---:|
| Anne | 150–450 | 31.77% | 31.93% | +0.52% |
| Anne | 450–750 | 25.30% | 27.44% | +8.45% |
| Anne | 750–1050 | 33.10% | 35.82% | +8.20% |
| Rudger | 150–450 | 32.01% | 32.17% | +0.51% |
| Rudger | 450–750 | 28.57% | 30.81% | +7.83% |
| Rudger | 750–1050 | 39.50% | 42.56% | +7.74% |
| Kinds | 150–450 | 33.97% | 34.16% | +0.53% |
| Kinds | 450–750 | 32.00% | 34.77% | +8.67% |
| Kinds | 750–1050 | 44.49% | 47.99% | +7.86% |
| Shilka | 150–450 | 34.36% | 34.55% | +0.55% |
| Shilka | 450–750 | 30.81% | 33.24% | +7.90% |
| Shilka | 750–1050 | 32.54% | 35.70% | +9.70% |
| Amia | 150–450 | 33.43% | 33.61% | +0.55% |
| Amia | 450–750 | 34.26% | 36.63% | +6.92% |
| Amia | 750–1050 | 28.03% | 30.82% | +9.94% |
| Huey | 150–450 | 32.67% | 32.84% | +0.52% |
| Huey | 450–750 | 28.21% | 30.55% | +8.28% |
| Huey | 750–1050 | 39.22% | 41.68% | +6.25% |
| Eusis | 150–450 | 32.73% | 32.90% | +0.52% |
| Eusis | 450–750 | 27.06% | 29.46% | +8.87% |
| Eusis | 750–1050 | 33.35% | 35.82% | +7.40% |
| Nei | 150–450 | 40.19% | 40.29% | +0.25% |
| Nei | 450–750 | 36.56% | 38.42% | +5.07% |
| Nei | 750–1050 | 37.09% | 38.66% | +4.22% |

Busy gameplay is still below full speed. The earlier title/music benchmark
is a different workload; its much higher percentage does not describe these
busy scenes. Raw cycles and inputs are in `validation_cp24/FIXED_GUEST_SPEED.json`.

## Verification

- All eight adventures: full CPU/bus/EEPROM and memory states match sealed
  CP21 over 1,050 guest frames. Only optimization history is excluded.
- 19,456 differential copy/fill boundary cases: exact CPU, RAM and code
  generation counts across event limits, counters, odd addresses, overlaps,
  RAM page edges, wrapping and VDP fallback.
- Independent original renderer: 200 random VDP states, 1,600 aligned mixed
  states, 120 single-plane/window states, 1,440 incremental mutations, and
  real incremental runs of all eight games have zero differing pixels.
- Actual ARM/Thumb ROM: 700 generated sound frames each for Anne song 0x81
  and Nei song 0x84 match our CP22 PCM exactly, before FIFO playback.
- Actual ARM/Thumb busy scenes: Anne and Nei CPU/bus/all memories and RAM
  code generations match our CP22 at guest frames 150, 450, 750 and 1,050.
  CPU optimization history and renderer/event diagnostics are excluded.
- All eight EEPROM slots power-cycle and reload a non-default save word;
  PS1 and Gaiden mapped SRAM writes/reloads preserve other save areas.
- Static collection/cartridge checks pass. Launcher, FM wave bank,
  PSII/III/IV and the PSC8 runtime match our CP22 byte-for-byte.
- Division by 12 and 18 is exact over 850,005 sampled 32-bit values each.

The detailed evidence and test scripts are included. The final rebuild matches every byte of the tested ROM; the build script
also checks its size and CRC.

## Space and rebuild

Genesis IWRAM ends at `0x03007968`: **24 bytes** below its reserved limit.
Genesis EWRAM ends at `0x0203AE20`: **12,768 bytes** below its reserved limit.
Do not add hot code or data without first making room. PSC8 remains unchanged.

`tools/build_cp24.sh /absolute/path/to/Release48` restores the included LLVM18
Linux toolchain as needed, then builds and checks CP24. Source and assets are
complete; no checkpoint overlay chain is required. See `BUILD_CP24.md`.
Keep the accepted generated AOT source: raw regeneration is guarded because
it can discard earlier verified native fusions. `aot/native_spans.py` also
applies the new byte-loop fusions at the generator template level.

## Next work

Profile actual busy guest frames. One Anne frame before this pass cost
2,665,068 GBA cycles: dirty collection 15.2%, pixel composition 10.5%, plane
cell decoding 5.2%, with most remaining work in 68000 translated routines,
RAM reads/writes and dispatch. Common object/text routines around guest
0x2B9CA, 0x2BCF8 and 0x2BC0E are useful next native-operation candidates.
Re-profile the final ROM before choosing a new target. Preserve event limits,
PCM, CPU state, framebuffer semantics, saves, every game and the 32 MiB size.
