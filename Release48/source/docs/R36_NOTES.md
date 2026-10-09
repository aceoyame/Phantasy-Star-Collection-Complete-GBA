# R36 — spaceship selector, travel and sabotage

Final native build: `RUN_THIS_R36_PSC_SPACESHIP.gba`, 32 MiB PSC packaging. This continues the recovered final R35. No emulator/hardware playthrough or finishability certification has been performed.

## Implemented

- Original InsideSpaceship $0D and SpaceshipSabotage $0E destination mask priority, current-world exclusion, source names and solar-system graphics. Cancel is source bit4; Speak/Camp confirm. Original planet palette blink, confirmation text and all six source entry/takeoff/landing tables are retained.
- Real map continuations use existing native AD resume bytes. Surface ascent runs 419 updates, station ascent 228, sabotage 112; normal surface whiteout retains 60 VBlanks. Source-permitted skipping remains, and sabotage's 112 updates cannot be skipped. All motion uses original 16.16 values and overworld wrapping/bounded-map clamps.
- Both small ship and Landale source art/controllers, plus all six destination planet mappings. The actor bank appends 54 poses / 41,248 bytes and retains the complete 107,712-byte R35 prefix; total 388 poses / 148,960 bytes.
- Space pass preserves 300 planet updates, the initial source publish/fade, original foreground scroll and star/ship/planet palette cycles. The source caption uses CRAM47 and appears after 191 updates. Fixed stars and flattened ship art reuse existing presentation scratch; the cache produced the same pixels as full composition in five inspected frames before the caption-timing correction.
- Landing preserves the Y-only wrapped camera move, original Start/Speak skip points and 257 deceleration updates. Final entries/cancellation rebuild the real map and party.
- Sabotage retains original 7A/7C/7D/7E panels, actual Tree35 entry2 F7 continuation, source ChaosSorcr flag $71 and genuine battle8 request. Existing map $18C RunEvent $29 requests CrashLanding $800F after the successful battle return. Defeat returns to the title through the existing battle runner. No fake victory or direct forced crash.
- Scripted flight enables the existing HBlank field camera explicitly. Selector, space pass and sabotage panels disable that publisher before reusing bitmap pages, preventing field ring writes over the new screens.

## Verification

- ARM/LLVM18 build passes. Module: 24,413,216 bytes. Native slot free: 752,608 bytes. Final image: 33,554,432 bytes.
- 27 accepted audio files and first 8 MiB PSC shell compare byte for byte with R35. No audio changes, blanket asset regeneration or project digests.
- IWRAM end 03006C88 (120 bytes before IRQ reserve). EWRAM BSS end 02029FC5 (59 bytes before presentation arena). EWRAM object names and sizes are unchanged.
- Existing R31 Rune/Dorin, R34 Psyco Wand/Zio and R35 Wren/crash/Raja probes pass with the extended actor bank.
- R36 ASan/UBSan host probe passes original selector priority/cancel/confirm, literal ship timers, all three ascent modes for both ships, source-permitted skips, twelve ship/planet space-pass cases, landing, real map requests, actual sabotage dialogue seams and battle handoff. An independent instruction reference matches every camera/velocity/ship position across 1,638 ascent/whiteout frames.
- Hardware DMA, actual map loading, dialogue display and combat are stubbed in the host probes. Review PNGs come from native indexed-pixel composition with actual source art, not emulator screenshots. Hardware cadence and normal-save route correctness remain unverified.

## Remaining blockers

Dispatch coverage is now 156/161 ordinary events and 15/33 real cutscenes; all 54 used map updates, 98 used conditions and 127 used managers have native paths. A dispatch count is not behavior certification.

The next source scenes are Landale $10 and DarkForce1Defeated $11. First Rykros arrival still enters the unimplemented original Rykros cutscene $1B and is intentionally blocked, rather than skipping its story effects. Ordinary events $2C, $46, $47, $48 and $86, later battle/controllers, Profound Darkness and the ending remain open.

Next verify a normal save through Wren → spaceship sabotage → real Chaos Sorcerer battle → crash/Raja, then implement the remaining story scenes. Keep saving checkpoints. Preserve accepted audio and the PSC shell; no frame skipping, audio decoupling, cycle scaling, dropped guest updates or story-flag bypasses.

## Build and recovery

`python source/build_r36.py` only regenerates the small append-only actor pose bank; the large existing asset banks are reused.

If the compiler is missing: `python source/tools/restore_clang18.py --out /absolute/path/compiler`. This pins official Ubuntu LLVM18 packages, validates dependency archives and performs an ARM smoke build. Add `/absolute/path/compiler/bin` to PATH and `/absolute/path/compiler/root/usr/lib/x86_64-linux-gnu` to LD_LIBRARY_PATH. The compiler recovery script was tested against the original dependency packages; the complete seven-archive source chain was also restored and every final increment file and launcher compared exactly. Dependency digests do not inspect or regenerate game assets/audio.

Source restoration uses these archives, in order. Final R36 checkpoint 04 is cumulative against final R35, so R36 checkpoints 01–03 are not needed.

| Archive | Saved reference |
|---|---|
| PSIV_R32_CHECKPOINT_01_2026-09-30.zip | libfile_1470d4c0365c8191b65f8d032d37268f |
| PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip | libfile_25207882f74081918cfecd66ac195ef6 |
| PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_1aa0718507488191ac101a8b148da823 |
| PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_94852fb0d52c81918fe156a3ae0365b1 (version 1) |
| PSIV_R35_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_57a770bbb258819192aacc5b80e05679 (version 1) |
| PSIV_R36_CHECKPOINT_04_INCREMENT_2026-10-01.zip | Final checkpoint accompanying these notes |

Materialize those exact ZIPs, then run `python source/tools/restore_r36_source.py --archives /absolute/path/archives --out /absolute/path/R36`. The script strips each archive's revision directory before overlaying, tests each ZIP CRC and checks the restored source. Linked R35/R36 maps and all source probe evidence are in build_artifacts.

The full R35 ROM was recovered and saved successfully as PSIV_R35_ROM_2026-10-01.zip, libfile_54b8e31ef06c8191a95f9789b0c2edaf. R36 is delivered as the complete ROM ZIP; no delta reconstruction is needed.
