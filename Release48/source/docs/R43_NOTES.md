# R43 — Elsydeon cave and reunion

Continues the verified R42 source. Cave entry1D, Elsydeon1E and reunion1F now execute their original story progression. The complete 32 MiB PSC ROM retains the accepted 8 MiB shell and 24 MiB native module slot. Full-game finishability remains unverified.

## Changes

- BeforeElsydeonCave: original lead walking route, door cue,120 map updates, panels18E/128 and Tree20 entry30. Saves all five party IDs at ED54..58, sends Chaz alone, recovers his stats plus vehicle charges, sets D8 and queues the real InnerSanctuaryB1 reload at cells3C/32 facing4.
- Elsydeon: original Tree20 entry31 and three F7 continuations, entry32, all original 21 panels and authored holds. Preserves the different white-flash masks, palette save ranges and source-clock tone/restore loops. Only the authored Event_RunDialogue call emits a selection cue; inline RunText continuations keep their source behavior. D9 follows final dialogue. Equips sword77, clears Chaz's left hand, restores all five saved party IDs and queues the real sanctuary reload facing0. Native derived stats update immediately, applying the disassembly's documented optional fix so the acquired sword is effective.
- Reunion: source Rune/Rika NPC targets and static Rika movement, Tree20 entry0, full original 300-update space pass and real MotaSpaceport reload at cells3C/24. B2 phase1 resumes after that map load, with MachineCenter music, all five original companion targets, lead walking, panels13F/13E/A7 and Tree39 entry1. DA follows dialogue and queues the second real MotaSpaceport reload at3C/36. Phase2 equips the original Hahn/Gryz/Demi/Raja/Kyra loadouts and recomputes native derived stats.
- Seven original character NPC types now follow source auto-input axis priority, wrapped routes and16-pixel steps, with state in the existing source records. They draw the original consecutive character sheets. Existing Hahn/Alys ROM-sheet art uses the same helper. New helper bodies stay in ROM to fit IWRAM; no source update is dropped.
- Shared RecoverStats now restores HP/TP, clears status, refills all eight skill-use counters and all three vehicles' charge counters. The prior helper omitted those counters and overwrote unrelated combat/element bytes; it now preserves those bytes as the original routine does.

## Verification and limits

ASan/UBSan scene probes execute production dialogue opcodes, party walking, R43 helpers, character derived-stat arithmetic and recovery with original dialogue bytes. Independent source references verify every F7/FF seam, original panel/cue order, authored holds and280 tone/restore palette ticks across all 64 words. Cave entry tests five possible late companions; reunion tests both actual map-load continuation phases and the full space pass.

Six zero-to-five-member recovery cases compare the entire64 KiB RAM against independent original write masks. Cave recovery independently verifies Chaz-only character writes and all vehicle counters. Original22-byte item records independently reproduce Elsydeon and all five reunion equipment effects, preserving every other character-record byte.

224 NPC movement cases cover seven types, all four source speeds, both axis orders, ordinary/wrapped/idle routes and4256 independently matched frames.144 original ROM poses/36864 bytes, including unchanged Hahn/Alys art types, match independent tile-order and nibble conversion. NPC animation uses the existing native party pose period; the movement reference validates position, facing and source step counters. Cached actor assets and source character sheets were retained.

All 27 original panels (1036800 pixels) match independent original-art sampling through unchanged production Nemesis/Enigma decoders and native fallback compositors. The source-art preview was visually inspected. It is not an emulator screenshot or a stacked-panel UI test.

All 21 inherited R31–R42 scene/vehicle/map/story/panel/pose/art checks pass. Input/glyph display, panel publications, generic NPCs, map loading and hardware remain fixtures; dialogue input waits use zero fixture frames. There is no normal-save emulator/hardware full-game playthrough, physical audio timing test or maximum call-chain stack proof.

The native module is 24477512 bytes, leaving 688312 bytes in its slot. The ROM is 33554432 bytes. IWRAM ends 03006CB0,80 bytes before the reserved stack boundary; EWRAM BSS ends 02029FC5,59 bytes before the existing presentation arena. EWRAM object names/sizes are identical to R42. New scene frames are32–48 bytes and NPC helpers0–48 bytes; these individual compiler frame sizes do not prove total call-chain stack use. All 27 accepted audio files, the8 MiB PSC shell and full181920-byte/494-pose actor bank are byte-identical to R42. No immutable asset regeneration or project hashes.

Static coverage is159/161 ordinary executors,31/33 real cutscenes,54/54 map-used updates,98/98 conditions and127/127 managers. A handled ID represents a code path, not finishability certification.

## Remaining work

Ordinary events MonsenEarthquake2C and DyingBoy86; ProfoundDarkness20 and Ending21 scenes. Composite/mechanical actors, movement-heavy battle controllers, Profound Darkness combat phases, ending/credits and a normal-save full-game route still need completion/verification. Continue original-source terminal progression and keep checkpoints. Do not introduce Butano, frame skipping, dropped source updates, decoupled audio, cycle scaling or invented story/battle bypasses.

## Build and recovery

Build with python source/build_r43.py. It reuses immutable assets and disables project hashes. Recover LLVM18 with source/tools/restore_clang18.py; put compiler/bin on PATH and compiler/root/usr/lib/x86_64-linux-gnu on LD_LIBRARY_PATH. The current compiler is /workspace/scratch/d7616bf13afc/r41_compiler.

Run inherited probes sequentially through check_r42_leroof_art.py, then check_r43_scenes.py and check_r43_panels.py. Panel checks need the generated scene fixture. Run check_r43_preservation.py --baseline /absolute/path/R42/source and audit_r43_subsystems.py. Evidence is in build_artifacts.

Recovery chain (overlay revision roots in order using tools/restore_r43_source.py):

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
| PSIV_R43_CHECKPOINT_04_INCREMENT_2026-10-01.zip | See final R43_NOTES_2026-10-01.md handoff | 0 |

Earlier R43 checkpoints are cumulative against R42 but are not needed once final04 is available. Archives are written atomically, CRC-tested, saved sequentially and read back for exact byte comparison. All 13 chain archives pass CRC. All 968 source files restore byte for byte, remain exact after compilation, and a fresh native rebuild reproduces the24477512-byte module and33554432-byte ROM exactly. Final04 includes the recovery evidence and source notes; those evidence-only updates are overlaid onto the recovery tree and compared again before delivery.
