# R41 — Eclipse Torch theft and use

Continues the verified R40 native offline C GBA project. New ordinary events46 and47 connect the Torch theft and its use at the Air Castle trees. Complete32MiB PSC packaging:8MiB accepted shell plus24MiB native module slot. Full-game finishability remains unverified.

## Implemented

- EclipseTorchStolen46: original Tree37 entry32 and all three F7 continuations, distinct original Bishop art, party facing, source X-then-Y shortest12-bit-wrap camera, Teleport/BlackBlood/Suspicion/Temple cues and each dialogue selection cue. Source actor slots8–17 implement appear, idle, fractional ghosts, disappear and light-circle phases with original timers and animation. Expired circles reactivate when the source rewrites their slots. Source field Torch/block clear without inventory modification. Bishop and Tree38 restore before flag98.
- EclipseTorchUsed47: existing source entrance conditions retained. Each vehicle disembarks into the original stacked party carriers and restores palette/music. Original panel10D appears over the native field snapshot. Two source flashes use Main_Frame_Count phase gating, followed by nine original ash actors and authored waits. Both exact16-word6100→6960 chunk copies and original Tree37 entry2A precede flag9C. Torch stays in inventory until the existing R40 Bishop exchange.
- Original object controllers280/284/288/28C/290/294/338 translated into existing64-byte records. Original raw initialization bits, fractional movement, frame-duration boundaries and transitional appear→idle pose retained.75 missing source poses appended,21440 bytes; all160480 prior actor bytes and419 header rows preserved. Bishop8194 uses original1A5F38 art rather than Saya's existing art at the shared base tile.
- No accepted audio regeneration, new RAM globals, frame skipping, source-update drops, cycle scaling, decoupled audio, Butano or flag/battle bypasses.

## Validation and limits

R31/R32/R34/R35/R36/R37/R38/R39/R40 regressions passed. R41 scene probe runs unchanged native event/actor headers under ASan/UBSan with original ROM dialogue/action bytes. Independent instruction references match1680 actor frames, six camera routes including wrap/tie cases, and all64 palette colors across168 flash VBlanks/four phases including counter wrap. Tests cover walking and vehicles1–3, all five stacked carrier positions, original dialogue seams and cue intervals, expired-slot reactivation, pose upload bounds, exact chunk copies and adjacent sentinels, final flags and Torch preservation.

Independent mapping/tile sampling checks all75 appended poses/42880 pixels and original panel10D/38400 pixels against source art. Both previews were inspected. Actor atlas is a grayscale source-art check, not a screenshot of final palettes. Native panel decoding/composition executes under ASan/UBSan.

Scene input/rendering, general NPC wandering, map loading, native background snapshot and live panel/hardware lifecycle remain fixture boundaries. No normal-save emulator/hardware full-game route, physical audio timing test, maximum call-chain stack proof or final-game certification has run.

Static coverage:158/161 ordinary executors,25/33 real cutscenes,54/54 map-used updates,98/98 conditions and127/127 managers. Counts show code paths, not certified finishability.

## Remaining work

Ordinary executors: MonsenEarthquake2C, MeetingLeRoof48 and DyingBoy86. Cutscenes: AeroPrism1A, first Rykros arrival1B, LeRoofAgain1C, BeforeElsydeonCave1D, Elsydeon1E, Reunion1F, ProfoundDarkness20 and Ending21. Continue the original AeroPrism/Rykros/LeRoof progression next. Composite/mechanical actors, movement-heavy battle controllers, Profound Darkness phases, ending/credits and a normal-save full-game playthrough remain open.

## Build and recovery

Build with python source/build_r41.py; immutable asset regeneration and project hashes are disabled. Recover LLVM18 with source/tools/restore_clang18.py and add compiler/bin to PATH and compiler/root/usr/lib/x86_64-linux-gnu to LD_LIBRARY_PATH. Current recovered compiler is /workspace/scratch/d7616bf13afc/r41_compiler.

Run source/tools/check_r41_scenes.py, check_r41_panels.py and check_r41_poses.py after the inherited probe chain. Run check_r41_preservation.py --baseline /absolute/path/R40/source and audit_r41_subsystems.py. Evidence is in build_artifacts; linked RAM bounds and immutable byte comparisons are in R41_PRESERVATION.json.

Restore these exact archives in order with python source/tools/restore_r41_source.py --archives /absolute/path/archives --out /absolute/path/R41. Final04 is cumulative against R40; intermediate R41 checkpoints are unnecessary for final recovery.

| Archive | Saved reference |
|---|---|
| PSIV_R32_CHECKPOINT_01_2026-09-30.zip | libfile_1470d4c0365c8191b65f8d032d37268f, version0 |
| PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip | libfile_25207882f74081918cfecd66ac195ef6, version0 |
| PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_1aa0718507488191ac101a8b148da823, version0 |
| PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_94852fb0d52c81918fe156a3ae0365b1, version1 |
| PSIV_R35_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_57a770bbb258819192aacc5b80e05679, version1 |
| PSIV_R36_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_63c577b95eb481919e7585f2995178eb, version0 |
| PSIV_R37_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_5954b2ae5c9481919801b58af04843b7, version0 |
| PSIV_R38_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_389ef8e1ad748191a4e6d017cf3a54f4, version0 |
| PSIV_R39_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_f72ba63dc7448191a52e2845c7b50834, version0 |
| PSIV_R40_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_8a57256f57a081918f9144284402be2a, version0 |

| PSIV_R41_CHECKPOINT_04_INCREMENT_2026-10-01.zip | Assigned after recovery verification; external R41 handoff records the saved ID. |

All archive writers close and CRC-check temporary files before atomic publication. Saved copies are read back and compared byte for byte. R41 checkpoint01 libfile_5807b009f9f0819187a9be5348b8c774/version0 and checkpoint02 libfile_d770a5979c1081918f1bec4a249bec5e/version0 passed readback.01 contains one redundant unchanged audio include because the original local baseline was truncated; the authoritative archive repaired the baseline. All932 baseline source files now match the R40 archive chain.

PSIV_R41_ROM_2026-10-01.zip contains the complete ROM and needs no source reconstruction. R40 fallback ROM libfile_1b39e49106e08191bcffe4f73fd78f7d/version0. External R41_NOTES_2026-10-01.md records final saved IDs, build bounds and recovery results after assignment.

Final build: module24467400 bytes;698424 bytes free in the24MiB slot. ROM33554432 bytes. IWRAM ends0x3006c90,112 bytes before IRQ reserve; EWRAM BSS ends0x2029fc5,59 bytes before presentation arena. All EWRAM object names/sizes match R40. All27 accepted audio files, first8MiB shell and full160480-byte actor prefix pass byte comparisons.

Saved ROM: libfile_03d0340575d88191adc18b7a9d0bedc7/version0,13602157 bytes. Saved copy CRC and complete33554432 ROM bytes matched. CP03: libfile_b4e63f68b50c81919b75e363714db888/version0,1507614 bytes.

Final recovery passed: all11 archive CRCs and all946 current source files restore byte-exact. The restored source reproduced the24467400-byte module and complete33554432-byte ROM byte for byte; post-build source still matches. Final04 contains this recovery report.
