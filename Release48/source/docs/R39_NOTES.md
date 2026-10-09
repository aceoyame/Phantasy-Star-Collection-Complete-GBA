# R39 — Lutz, Air Castle and Seth

Final native build: RUN_THIS_R39_PSC_LUTZ_AIR_SETH.gba, complete32MiB PSC ROM. Continues the verified R38 source. This revision advances story progress but full game finishability remains unverified.

## Implemented

- LutzRevelation $14 / $77896: incoming Tree20 ($1EDCA0) entry2A,30 map-only updates, original Character1 walking to200/1E0, StairsDF and exact all64 nibble fade with delay2. B0 phase0 requests real sanctuaryB1 map16F / previous16E at cells3C/4C, facingC/alignment0 and cleared map-load bit3.
- After the real reload, the existing production BG32-only nibble restore preserves OBJ colors and the source final empty3-VB hold. Character1 walks to1E0/1F0; Kyra moves independently16pixels right; source turns4/8/C/8/4 publish oneVB each and hold40/10/10/10/60 full-object updates. Kyra returns to1F0/1F0 and independent-party bit0 clears.
- Original Tree20 entry2F has one F7. The complete saved segment resumes after source InitVRAMAndCRAM/PalFadeIn, panel9E and a100-VB authored hold. Actual dialogue actions create9F/100..106. Flag97 precedes B0 phase1's real final16F/16E reload at cells3C/48, face0/alignment8. Native map reload installs the final aligned party.
- FindingAirCastle $15 / $77A2E: panel18A, incoming Tree14 ($1E99F0) entry18 from original spaceship mapD4, source dialogue5 and flag99. Tail handoff runs the existing R36 native spaceship selector/confirm/cancel and AD flight/map continuations. AirCastleFound selects original priority maskDC. No invented destination, story bypass or automatic victory.
- MeetingSeth $19 / $77EAC: panel118, explicit Tree42 ($1FC920) entry0 with actual panel actions18D/119, source dialogue5, party slot5=Seth10 and ATTACK in used macros. B0 phase2 requests actual Motavia0 / previousFFFF at cellsEE/136, face0/alignment0. SethJoined flagC1 is set only after that reload.
- B0 reuses existing continuation bytes. No new EWRAM globals, graphics-bank edits or accepted audio changes.

Correction to the R38 handoff: $15 is FindingAirCastle; MeetingSeth is $19. The source CutscenePtrs table owns these IDs.

## Verification

- LLVM18 native ARM build succeeds. Module24435848 bytes;729976-byte headroom in the24MiB native slot. Complete PSC ROM33554432 bytes. First8MiB PSC shell and all27 accepted audio files are byte-identical to R38. Full160480-byte/419-pose actor bank is also unchanged. No asset regeneration and no project hashes.
- IWRAM end03006C90,112bytes before IRQ reserve. EWRAM BSS end02029FC5,59bytes before presentation arena. EWRAM object names and sizes exactly match R38. Compiler frames: new handlers16–40bytes; existing party walking160bytes. These static sizes are not a maximum call-chain stack proof.
- R31/R32/R34/R35/R36/R37/R38 focused regressions pass, including599-frame crash,1638-frame flight,465-frame Dezo camera/snow and48-frame copied-Esper reference.
- R39 ASan/UBSan probe runs unchanged native scene headers with actual original dialogue/action bytes, real production party-walking and palette functions, party carriers/macros, real destination selector, map request/continuation state and flag order. Independent auto-input/16pixel-step/shortest-wrap/axis-priority/party-chain reference matches16cases/480frames across all four source speeds and movement control combinations. Independent palette arithmetic matches87VBlanks, including BG-only restore and termination hold.
-13 original R39 scene panels pass ASan/UBSan with unchanged production Nemesis fallback, Enigma decoder, panel geometry, layer renderer and compositor. Every499200 pixel matches independent Python decoding/tile sampling. Largest art7104bytes within16384-byte scratch; largest map204cells within2048-cell scratch. The source-art contact sheet was inspected. Native precompiled lookups are deliberately absent in this probe, so it covers the runtime fallback.
- Scene probe map loading, general NPC wandering, tile animation, dialogue display timing and hardware are fixtures. The final native map loader is connected but normal-save/hardware route and audio/frame timing remain untested. Panel probe does not certify the actual stacked-panel UI/display lifecycle.
- Static dispatch coverage now156/161 ordinary events,22/33 real cutscenes,54/54 map-used updates,98/98 conditions and127/127 managers. Counts indicate native code paths, not retail behavior or finishability certification.

## Next work

Verify a normal save through R38's clinic/Kyra and R39's Lutz/Air Castle/Seth scenes. Next source scenes: LashiecDefeated $16, DarkForce2Defeated $17, GumbiousBishop $18 and first Rykros $1B. Ordinary events2C/46/47/48/86, later composite/mechanical field objects, movement-heavy combat controllers, Profound Darkness phases and ending/credits remain open. Full game is not certified playable to completion.

Keep persistent checkpoints. Preserve accepted27-file audio, first8MiB PSC shell and current actor bank. No Butano, frame skips, decoupled audio, cycle scaling, dropped source updates or story-flag bypasses. Authorized packaging is8MiB shell plus24MiB native module,32MiB total.

## Build and recovery

Working tree: /workspace/scratch/d7616bf13afc/r39_work/R39. Untouched restored baseline: /workspace/scratch/d7616bf13afc/r38_work/R38. The scratch tree had reverted to R36 at the beginning of this turn; all R38 source was recovered from the eight saved archives before edits. Compiler18 was recovered from pinned cached packages and its ARM smoke test passed.

Build: python source/build_r39.py. It reuses all immutable asset banks with PS4_SKIP_ASSET_REGEN=1 and PS4_NO_HASH=1. Compiler: python source/tools/restore_clang18.py --out /absolute/path/compiler (optional --archives /absolute/path/pinned-packages for cached recovery). Add compiler/bin to PATH and compiler/root/usr/lib/x86_64-linux-gnu to LD_LIBRARY_PATH. Current compiler: /workspace/scratch/d7616bf13afc/toolchain_download.

Re-run source probes in order: check_r31_story.py, check_r32_vehicles.py, check_r34_scenes.py, check_r35_scenes.py, check_r36_spaceship.py, check_r37_scenes.py, check_r38_scenes.py, check_r39_scenes.py, check_r39_panels.py. Then check_r39_preservation.py --baseline /absolute/path/R38/source and audit_r39_subsystems.py. All are in source/tools.

Restore these exact archives in order; R39 final04 is cumulative against R38, so R39 checkpoints01–03 are unnecessary for final recovery.

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
| PSIV_R39_CHECKPOINT_04_INCREMENT_2026-10-01.zip | Assigned after final recovery verification; external handoff below is current. |

Run python source/tools/restore_r39_source.py --archives /absolute/path/archives --out /absolute/path/R39. It checks each ZIP CRC, strips revision roots and overlays one complete source directory. Final source notes precede assignment of final delivery IDs; the external R39_NOTES_2026-10-01.md includes them.

Final ROM ZIP contains the complete32MiB RUN_THIS_R39_PSC_LUTZ_AIR_SETH.gba. No source/delta reconstruction is needed to use it. R38 fallback: PSIV_R38_ROM_2026-10-01.zip, libfile_b8cbef1338d88191b5c0b34ce0035974, version0.

Intermediate checkpoint01: libfile_d00330674ba0819197a6d83f2fc1e227, version0,206847bytes. Checkpoint02: libfile_33e6b9fb67948191bf5590cc31a6cd78, version0,1012208bytes. Checkpoint03: libfile_a18630d6f5d48191a69b7a2fc267b8c6, version1,980725bytes. Its initial incomplete version0 was replaced; version1 was read back and passed CRC and exact-byte comparison. Checkpoints01/02 and the saved ROM were also read back and passed CRC/exact comparison. The checkpoint writer now uses a temporary ZIP and publishes the final path only after closing and CRC verification. Final04 includes final recovery evidence.

Recovery verification: all nine source archives restored with valid CRC; all922 current source files matched byte for byte. Final overlay contains51 members including the manifest and this recovery report; all are checked after the final archive rebuild. ROM ZIP CRC and complete native GBA equality passed. Saved checkpoints01/02/03(version1) and ROM were independently read back byte-exact.
