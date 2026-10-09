# PSII 21-program gain pass (CP07) – CP08 audit

**Result: the CP07 pass is kept unchanged in CP08.** No PSII audio byte changed in CP08. Every correction is a flat
per-sample amplitude scale (no filtering or EQ), bounded to ±3 dB, applied only to FM programs used by music with
correlation ≥ 0.8 against a source YM2612 render (`audio/ps2_source_gain/build_ps2_source_gain.py`).

## Source facts confirmed from the PSII disassembly
* PSII music uses the 68k pre-SMPS driver (`SoundDriverBase` 0xB8000). The FM instrument table is `FMInsPtrs` at 0xB8568
  (UE ROM), 26 bytes per patch in register order `B0 B4 40 48 44 4C 30 38 34 3C 50 58 54 5C 60 68 64 6C 70 78 74 7C 80 88 84 8C`.
  Music programs 0x40–0x68 are instrument indices 0–40, so the PSC program numbers in the audit are the source instrument IDs.
* Silent Zone (song 2): track 0 = PCM drums, FM ch0 = program **0x46** (track vol 20), ch1 = **0x49** (vol 12),
  ch2 = **0x4B** (vol 26). The sequence and program IDs are intact. The "missing instrument" report is a balance issue,
  not missing notes.
* The drums are PCM (`PCMDrums`, `sound/ps2.pcm_drums.asm`), not FM patches. **No percussion program has any gain correction**:
  the audit's FM-render method gives correlation 0.02–0.06 for them, so it has no valid measurement to apply.

## Evidence tiers for the 21 corrections
| Tier | Programs | Basis |
| --- | --- | --- |
| Fully supported (corr ≥ 0.9, measured gap ≤ 3 dB, so the whole correction was applied) | 0x45, **0x49**, 0x4A, **0x4B**, 0x4C, 0x50, 0x52, 0x55, 0x5B | measurement and correction agree |
| Direction supported, size capped (measured gap > 3 dB; −3 dB applied) | 0x40, 0x41, 0x4D, 0x59, 0x5A, 0x63, 0x67 | the true gap is larger than applied. For 0x59/0x5A/0x63/0x67 the source render is near-silent at the test note (head RMS 64–928 against a typical 4000–7000), so the size of the gap is not trustworthy, only its sign |
| Weak correlation (0.80–0.87) | **0x46**, 0x4F, 0x51, 0x5C, 0x62 | plausible but low-confidence |

Silent Zone therefore has two well-supported corrections (0x49 +1.7 dB, 0x4B −1.4 dB) and one low-confidence one
(0x46 +2.7 dB).

## Independent re-check attempted
I built a YM2612 renderer from ymfm (`render.cpp`, `measure.py`) to reproduce the source side from the patch bytes.
It could not reproduce the audit's alignment: the PSC samples are recorded at notes that do not map 1:1 to the audit's
`fnum 1584` render, and the audit's alignment and windowing parameters were not handed over. So this re-check neither
confirms nor refutes individual numbers, and I did not use it to change anything.

## Remaining
* Listen to Silent Zone / Dezolis through the new PSII debug MUSIC TEST (hold L+R while launching slot II), with
  0x46 as the first candidate to revert if it sounds too forward.
* If percussion balance is wrong, compare the PSC drum samples against `PCMDrums` directly. That is a separate PCM
  comparison, not this FM method.
