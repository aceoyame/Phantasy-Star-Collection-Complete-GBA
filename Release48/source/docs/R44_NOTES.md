# R44 — Monsen earthquake and Alis' sword

Continues the verified R43 source. The two remaining ordinary events, MonsenEarthquake2C and DyingBoy86, now execute their original scene progression. The native module remains within the 24 MiB slot of the accepted 32 MiB PSC package. Full-game finishability remains unverified.

## Changes

- Monsen: incoming Tree15 (1E7D50), entry40 and all nine F7 continuations. The first 120-update horizontal quake and second 60-update loop retain the original tables, including its first two diagonal ticks followed by the horizontal table. All 45 GraveOpening cues retain four-tick spacing. Counter updates follow the source's different before/after-VBlank order in the two loops. Destinations and faces resolve Chaz, Gryz, Rika, Demi and optional Rune by party ID. Gryz's twenty facing updates retain both extra VBlanks, followed by the original map-only holds and four source villagers. Both 120-update object-only waits retain every object update and no map update. Villager records/art are cleared before the leading character walks to310/220. The final camera return updates both camera planes and ALL party/NPC records before flag60 PlateSystem is set.
- Dying boy: incoming Tree24 (1F0620), entry3E when AA AlisSword is already set, entry3B if A9 Culvers or the sword is missing, otherwise entry33 and its three F7 continuations. GetItem's occupied-byte count, first-match search and ReorderInventory's first-zero shift retain the source behavior, including the padding byte and irregular inventory cases. Only one sword is consumed. All 150 main updates retain the source one-pixel speed and simultaneous ID-based destinations. The optional companion priority is Hahn/Gryz/Demi/Raja/Kyra/Seth. Chaz is hidden during the dedicated334 actor's original variable-duration animation and restored before his single-object move. The final leading-character move updates all objects. Speed, movement flags and AA follow the source terminal order. No invented recovery/battle/story effects.
- Every original Event_RunDialogue/GetAndRunDialogue emits its authored Selection cue, including repeat/missing-prerequisite branches. Shared production RunText retains normal dialogue mode and F7/FF seams.
- Record motion carries source 8.8 step durations, 16.16 velocity/position and axis/chaining state in existing64-byte source records. Fixed-count scenes update occupied party records and existing NPCs on every authored tick. Native party art uses the established source-duration pose clock; the movement references verify raw positions, facing, step durations and chain destinations, not an instruction-by-instruction character animation lowering.
-58 original-source poses/21248 bytes append to the entire181920-byte R43 bank. Three Monsen villager art families and all ten334 frames use original Nemesis art/mappings and their source palette lines3/2. All old bank bytes are retained. The NPC initializer corrects the source spawn's temporary Y-destination typo before movement begins.

## Verification and limits

All25 checks pass: the21 inherited R31–R42 checks, both R43 checks, and the R44 pose/scene checks. Older fixture executables omit the new R44 actor dispatch via a compile-time guard; the native title and R44 probe enable the actual added dispatch and helpers. All inherited paths remain exercised.

The R44 ASan/UBSan scene probe executes the production ordinary dialogue wrappers and RunText, source-record movement helpers, both event implementations, actor dispatch/animation, pose lookup and OAM publication. Independent original-ROM bytes verify every F7/FF seam, Selection/GraveOpening cue order and authored holds. Three Monsen party cases cover missing Rune, Rune present and a reordered lead. Twelve dying-boy cases cover all six optional companions, no optional companion, repeat completion, missing Culvers/sword and a reordered lead.44 inventory cases cover every sword slot, duplicates, irregular holes, absence and the original padding-byte shift.

540 quake ticks compare both camera planes, clamp limits, fixed-point residuals and the before/after-VBlank source counters.128 motion cases cover all four source speeds, both axis priorities, chaining enabled/disabled, positive/negative/idle routes and1696 independently matched raw-record ticks.6600 simultaneous party-record ticks across the dying-boy cases match an independent source-motion reference. The334 actor's164 ticks match the original variable-duration sequence, first advance and terminal frame9/duration1. Actual added actor dispatch, source palette lines and cache-backed pose publication are checked. The final Monsen camera return verifies both planes, map-update order and live follower updates.

Every one of the42496 pixels in all58 appended poses matches independent original mapping-piece sampling, including flips and tile order. The grayscale pose preview was visually inspected; it is a source-index preview, not an emulator screenshot.

Input/page waits, glyph/panel display, ordinary NPC wandering, map loading and hardware remain fixtures. Dialogue input waits use zero fixture frames. There is no normal-save emulator/hardware full-game route, physical audio timing proof or maximum call-chain stack proof.

The native module is24509160 bytes, leaving656664 bytes in its slot. The full ROM is33554432 bytes. IWRAM ends03006CB0 (80 bytes before the reserved boundary); EWRAM BSS ends02029FC5 (59 bytes before the existing presentation arena). All EWRAM object names/sizes equal R43. Compiler individual frames range24–112 bytes for retained R44 helper bodies; individual frames do not establish maximum call-chain stack use. New code stays in ROM. All27 accepted audio files, the8 MiB PSC shell and entire R43 pose prefix are byte-identical. No blanket asset regeneration or project hashes.

Static coverage is161/161 ordinary executors,31/33 real cutscenes,54/54 map-used updates,98/98 conditions and127/127 managers. A handled ID is a code path, not finishability certification.

## Remaining work

ProfoundDarkness20 and Ending21 scenes; composite/mechanical actors, movement-heavy battle controllers, Profound Darkness combat phases, ending/credits and a normal-save full-game route still need completion/verification. Do not introduce Butano, skipped frames, dropped source updates, decoupled audio, cycle scaling or invented story/battle bypasses. Continue with checkpoints.

## Build and recovery

Build with python source/build_r44.py. It preserves accepted assets and disables project hashes. Recover LLVM18 with source/tools/restore_clang18.py; put compiler/bin on PATH and compiler/root/usr/lib/x86_64-linux-gnu on LD_LIBRARY_PATH. The working compiler is /workspace/scratch/d7616bf13afc/verified_compiler.

Run source/tools/run_r44_regressions.py sequentially; it writes all25 logs and R44_REGRESSIONS.json. Run check_r44_preservation.py --baseline /absolute/path/R43/source and audit_r44_subsystems.py. Evidence resides in build_artifacts. Follow tools/restore_r44_source.py to restore the14 required archives. R44 final increment04 is cumulative against R43; earlier R44 checkpoints are not needed once final04 is available. External R44_NOTES_2026-10-01.md records final IDs.

All archives are atomically written, closed, CRC-tested and renamed. Saved checkpoints are read back for exact byte comparison. All14 required archives pass CRC; all984 source files restore byte for byte, remain exact after compilation, and a fresh native rebuild reproduces the24509160-byte module and33554432-byte ROM exactly. Final04 includes this evidence; evidence-only updates are overlaid and compared again. R44_RECOVERY.json records these checks.

| Archive | Persistent file ID | Version |
| --- | --- | --- |
| PSIV_R32_CHECKPOINT_01_2026-09-30.zip | libfile_1470d4c0365c8191b65f8d032d37268f | 0 |
| PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip | libfile_25207882f74081918cfecd66ac195ef6 | 0 |
| PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_1aa0718507488191ac101a8b148da823 | 0 |
| PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_94852fb0d52c81918fe156a3ae0365b1 | 1 |
| PSIV_R35_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_57a770bbb258819192aacc5b80e05679 | 1 |
| PSIV_R36_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_63c577b95eb481919e7585f2995178eb | 0 |
| PSIV_R37_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_5954b2ae5c9481919801b58af04843b7 | 0 |
| PSIV_R38_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_389ef8e1ad748191a4e6d017cf3a54f4 | 0 |
| PSIV_R39_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_f72ba63dc7448191a52e2845c7b50834 | 0 |
| PSIV_R40_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_8a57256f57a081918f9144284402be2a | 0 |
| PSIV_R41_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_09028eda68a08191af56750ac89bece5 | 0 |
| PSIV_R42_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_a3ad8d1a4830819195f7b756365ea14e | 0 |
| PSIV_R43_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_a7397d881ca48191924ed76b07071e8f | 0 |
| PSIV_R44_CHECKPOINT_04_INCREMENT_2026-10-01.zip | See external R44_NOTES_2026-10-01.md | 0 |
