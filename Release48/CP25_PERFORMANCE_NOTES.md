# R48 CP25 — Adventure speed checkpoint (2026-10-08)

Run `Release48/binaries/RUN_THIS_R48_CP25_ADVENTURE_INDEX.gba`.
The complete collection remains 32 MiB. CRC32: `056427B8`.

## Accepted changes

- An 8,192-byte EWRAM tile-to-visible-band index replaces repeated plane and
  window reference scans when both full-screen vertical scrolls are aligned
  to eight guest lines. Changed name words add conservative references;
  rebuilding after 64 dirty collections removes stale references. Full
  redraws invalidate it. Nonaligned and column-scroll cases retain the
  original fallback, and sprite dirty detection retains its exact scanner.
- Changed tiles' band masks are unioned before expansion. Clipped line ranges
  use native bit masks. Power-of-two geometry uses shifts. Collect-dirty
  remains Thumb: the ARM trial was slower and was rejected.
- Name-table dirty detection now handles 64 KiB VRAM wrapping. Line-scroll
  invalidation covers all 240 guest lines. Independent pixel checks include
  these cases, mode transitions, stale references and both framebuffer pages.
- Native RAM byte-loop spans now use aligned halfword/word transfers when
  safe. Copy order remains forward: two-byte overlap uses halfwords, odd
  parity uses bytes, and words require equal word alignment. Fills align
  before storing packed repeated bytes. Guest event limits, flags, cycles,
  counters, bus writes and RAM-code generation counts remain exact.

No CPU timing, PCM, rendering cadence or cartridge content was reduced.
Geometry shifts alone had negligible measured benefit. The index provides
most of the gain; packed transfers add a smaller improvement. The cache is
built lazily only when incremental dirty work needs it.

## Identical guest work and inputs

Baseline is delivered CP24 (`F35DA10F`), using its recorded native cycle
counts. Both builds ran in unmodified mGBA 0.10.5 with identical guest frames,
inputs and song command. Speed is guest frames divided by equivalent GBA
frames (280,896 native cycles each), not wall-clock or an artificial ROM-cost
multiplier. These are scripted scene windows, not whole-game or hardware
measurements. Raw cycles: `validation_cp25/FIXED_GUEST_SPEED.json`.

| Adventure | Guest frames | CP24 | CP25 | Relative gain |
|---|---:|---:|---:|---:|
| Anne | 150–450 | 31.93% | 31.95% | +0.06% |
| Anne | 450–750 | 27.44% | 29.41% | +7.17% |
| Anne | 750–1050 | 35.82% | 39.11% | +9.19% |
| Rudger | 150–450 | 32.17% | 32.19% | +0.06% |
| Rudger | 450–750 | 30.81% | 32.88% | +6.70% |
| Rudger | 750–1050 | 42.56% | 46.27% | +8.72% |
| Kinds | 150–450 | 34.16% | 34.18% | +0.07% |
| Kinds | 450–750 | 34.77% | 37.42% | +7.61% |
| Kinds | 750–1050 | 47.99% | 52.23% | +8.83% |
| Shilka | 150–450 | 34.55% | 34.57% | +0.06% |
| Shilka | 450–750 | 33.24% | 35.35% | +6.35% |
| Shilka | 750–1050 | 35.70% | 39.58% | +10.87% |
| Amia | 150–450 | 33.61% | 33.63% | +0.06% |
| Amia | 450–750 | 36.63% | 38.64% | +5.50% |
| Amia | 750–1050 | 30.82% | 34.09% | +10.63% |
| Huey | 150–450 | 32.84% | 32.86% | +0.06% |
| Huey | 450–750 | 30.55% | 32.74% | +7.18% |
| Huey | 750–1050 | 41.68% | 44.48% | +6.72% |
| Eusis | 150–450 | 32.90% | 32.92% | +0.06% |
| Eusis | 450–750 | 29.46% | 31.82% | +8.00% |
| Eusis | 750–1050 | 35.82% | 38.60% | +7.75% |
| Nei | 150–450 | 40.29% | 40.30% | +0.01% |
| Nei | 450–750 | 38.42% | 39.42% | +2.60% |
| Nei | 750–1050 | 38.66% | 40.09% | +3.69% |

Busy gameplay remains below full speed. Early windows improve only 0.01–0.07%;
this pass targets dirty graphics and native RAM work in later scenes.

## Verification

- All eight adventures: full guest CPU/bus/EEPROM and memories match sealed
  CP21 for 1,050 scripted guest frames; optimization histories are excluded.
- 38,912 native copy/fill differential boundary cases: 2,585,468,928 bytes of
  CPU/RAM/RGEN state match, including overlap, odd alignment, RAM boundaries,
  event limits and cycle-counter wrapping.
- Independent original per-pixel renderer: 200 random states, 1,600 aligned
  mixed states, 120 single-plane/window states, 1,440 incremental updates,
  6,144 index/fallback updates, and real incremental runs of all eight games
  have zero differing pixels.
- Final actual ARM/Thumb ROM: Anne and Nei CPU/bus/memories/RGEN match CP24
  at guest frames 150, 450, 750 and 1,050. Diagnostic histories are excluded.
- Final generated PCM: 700 sound frames each for Anne song 0x81 and Nei song
  0x84 match CP24 before FIFO playback.
- All eight EEPROM slots power-cycle/reload a valid nondefault word. PS1 and
  Gaiden mapped SRAM write/reload checks protect the other save areas.
- Static cartridge checks pass. Launcher, FM bank, PSII/III/IV and PSC8 runtime
  are byte-identical to CP24. Complete collection remains 33,554,432 bytes.
- Clean archive extraction rebuild is compared byte-for-byte with the tested
  final ROM. See `RESTORED_ZIP_REBUILD_EXACT.json` once packaging is complete.

Detailed evidence and reproducible checks are included. Earlier CP22/CP24
validation is retained for comparison.

## Space and next work

Genesis IWRAM ends at `0x03007968`: **24 bytes** below the reserved limit.
Genesis EWRAM ends at `0x0203CE28`: **4,568 bytes** below the reserved limit.
The index consumes 8,200 bytes including its validity/age words. PSC8 is
unchanged. Additional hot code or persistent caches need room made first.

Rebuild with `tools/build_cp25.sh`; see `Release48/BUILD_CP25.md`.
Do not raw-regenerate the accepted AOT source: the generator guard protects
previous native fusions. Copy/fill fusions also exist in `aot/native_spans.py`.

The remaining cost is mostly translated 68000 object/text routines, RAM access
and dispatch plus pixel composition. See `FRAME_PROFILE_FINAL.json` for one
busy Anne frame after this pass. Native operations around guest 0x2B9CA,
0x2BCF8 and 0x2BC0E remain useful candidates. Preserve guest event boundaries
and exact state, pixels, PCM, saves, all games and ROM/memory limits.
