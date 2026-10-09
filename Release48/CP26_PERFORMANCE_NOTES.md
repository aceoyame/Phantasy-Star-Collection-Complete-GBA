# R48 CP26 — Speed checkpoint (2026-10-08)

Final ROM: `Release48/binaries/RUN_THIS_R48_CP26_NIBBLE_NATIVE.gba`.
CRC32 `73A056D3`; complete collection 33,554,432 bytes.

## Accepted changes

The shared adventure nibble compositor at guest 0x2BC12 now runs complete
RAM byte blends as native ARM work. Both source nibbles retain the original
conditional transparent-color fallback to the destination. Original forward
read/write order, source/destination overlap, guest registers, flags, counters,
cycles, write counts and RAM-code generations are preserved. Each iteration
costs the original 96–128 guest cycles. The helper conservatively leaves short
event budgets, unsupported addresses and boundary-crossing work to the full
original instruction path. It adds no data cache or IWRAM helper code.

The generator fusion requires the complete original loop body to match;
changed instruction bodies are refused. Existing raw-regeneration guards remain.

Plane decoding skips flip and palette work for a completely transparent tile
row. Stale attributes are never consumed for zero nibbles. Palette/priority
attributes use equivalent bit extraction. Window decoding retains its previous
path: skipping empty rows there exceeded the IWRAM limit.

Rejected trials: the object-table native search passed state checks but slightly
slowed busy scenes; ROM-backed blending also slowed the accepted RAM version.
Neither is in the final runtime. An initial blend cycle-count error was caught
by full-state checks, corrected and revalidated before accepting measurements.
No guest timing, audio quality, rendering cadence, content or saves were reduced.

## Same guest inputs, actual GBA cycles

Baseline: delivered CP25 `056427B8`. Both builds execute the identical scripted
guest frames, inputs and song command. Rates use mGBA 0.10.5's native GBA clock,
with 280,896 cycles per GBA display frame, not host wall time or multiplied ROM
costs. These windows are not whole-game or physical-hardware measurements.
Raw cycle counts: `validation_cp26/FIXED_GUEST_SPEED.json`.

| Adventure | Guest frames | CP25 | CP26 | Relative gain |
|---|---:|---:|---:|---:|
| Anne | 150–450 | 31.95% | 31.98% | +0.09% |
| Anne | 450–750 | 29.41% | 29.78% | +1.24% |
| Anne | 750–1050 | 39.11% | 39.57% | +1.17% |
| Rudger | 150–450 | 32.19% | 32.22% | +0.09% |
| Rudger | 450–750 | 32.88% | 33.25% | +1.14% |
| Rudger | 750–1050 | 46.27% | 46.79% | +1.14% |
| Kinds | 150–450 | 34.18% | 34.21% | +0.10% |
| Kinds | 450–750 | 37.42% | 37.86% | +1.19% |
| Kinds | 750–1050 | 52.23% | 52.84% | +1.18% |
| Shilka | 150–450 | 34.57% | 34.60% | +0.09% |
| Shilka | 450–750 | 35.35% | 35.74% | +1.09% |
| Shilka | 750–1050 | 39.58% | 40.18% | +1.51% |
| Amia | 150–450 | 33.63% | 33.67% | +0.12% |
| Amia | 450–750 | 38.64% | 39.00% | +0.92% |
| Amia | 750–1050 | 34.09% | 34.63% | +1.57% |
| Huey | 150–450 | 32.86% | 32.89% | +0.09% |
| Huey | 450–750 | 32.74% | 33.16% | +1.29% |
| Huey | 750–1050 | 44.48% | 44.89% | +0.92% |
| Eusis | 150–450 | 32.92% | 32.95% | +0.09% |
| Eusis | 450–750 | 31.82% | 32.23% | +1.28% |
| Eusis | 750–1050 | 38.60% | 39.03% | +1.11% |
| Nei | 150–450 | 40.30% | 40.31% | +0.03% |
| Nei | 450–750 | 39.42% | 39.67% | +0.64% |
| Nei | 750–1050 | 40.09% | 40.29% | +0.51% |

This pass improves all 24 measured windows, but the gains are modest. Busy
adventures remain well below full speed. One profiled Anne guest frame falls
from 1,915,286 to 1,848,860 native cycles; that individual frame is not the
aggregate scene-window rate.

## Verification

- All eight adventures: 1,050 guest frames, complete CPU/bus/EEPROM and all
  guest memories match sealed CP21. Only optimization history is excluded.
- 45,120 blend boundary cases stream-compare 6,355,603,200 bytes of state
  against sealed CP25: 47 event budgets, four counters, five transparent values,
  overlap, odd/page/code-generation limits, RAM wrapping, ROM/VDP fallback,
  cycle wrap and full CPU/bus/EEPROM/memories/RGEN. No state-file hashes used.
- Independent renderer: 200 random states, 1,600 aligned mixed states, 120
  single-plane/window states, 1,440 incremental updates, 6,144 index/fallback
  updates and real incremental runs of all eight games have zero pixel changes.
- Final ARM/Thumb busy runs: Anne and Nei match CP25 CPU/bus/memories/RGEN at
  guest frames 150, 450, 750 and 1,050; diagnostic history is excluded.
- Final PCM: 700 sound frames each for Anne song 0x81 and Nei song 0x84 match
  CP25 byte-for-byte before FIFO playback.
- All eight EEPROM slots power-cycle/reload a nondefault word; PS1 and Gaiden
  mapped SRAM tests preserve the other save areas.
- Static collection checks pass. Launcher, FM bank, PSII/III/IV and PSC8 runtime
  are byte-identical to CP25. CPU/sound/mapper improvements from earlier passes
  are retained.
- Complete rebuild and clean archive-source rebuild are compared byte-for-byte
  to the fully tested payload. See the rebuild evidence in validation_cp26.

## Memory, rebuild and next target

Genesis IWRAM ends `0x03007974`: **12 bytes spare** below its reserved limit.
Genesis EWRAM remains `0x0203CE28`: **4,568 bytes spare**. No new cache was added.
Additional hot code or persistent data requires first making room.

Use `tools/build_cp26.sh`; see Release48/BUILD_CP26.md. CP25 originals and ELF
are sealed in cp25_base, with all older references retained. The generated
AOT source is accepted source; raw regeneration can discard earlier fusions.

The final Anne frame still spends about 11.2% in aotf70 (object/text routine),
7.1% in plane cell decoding, 5.3% in aotf71, 4.7% in CPU dispatch and 4.0% in
sound. Profile those bodies before choosing the next native operation. The
initial ROM-table scan was not the expensive part of aotf70. Preserve event
boundaries, complete state, pixels, PCM, saves, every game and all space limits.
