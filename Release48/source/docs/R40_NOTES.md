# R40 — Lashiec, Dark Force 2 and the Hydrofoil handoff

Final native build: RUN_THIS_R40_PSC_LASHIEC_DF2_HYDROFOIL.gba, complete 32 MiB PSC ROM. Continues the verified R39 source. Three more story continuations execute; a normal-save full-game route and finishability remain unverified.

## Implemented

- LashiecDefeated $16 / $77A68: original Tree22 ($1EF130) entry38 and its F7 continuation; original panels107–10C, source explosion/grave/music cues and authored holds. Three panel destroys share one presentation. The real Eclps Torch reward precedes the existing R36 world1 space pass. B1 phase0 requests real GumbiousF1 map162 / previousFFFF at cells4E/2C, facing4/alignment4. After map loading, original Temple music93 and Tree22 entry39 run; Tree38 ($1FA480) is restored.
- DarkForce2Defeated $17 / $77BDA: original Tree22 entry3B, all three F7 seams and four segments, original panels10E–116/A8, cues and holds. SnowstormGone flagA1 precedes exact Kyra9 removal from current party, all64-byte carriers, used macros and status. B1 phase1 requests real Dezolis1 / previousFFFF at cells174/E, facing0/alignment0, with source music9D. The native loader installs the four-person final party atBA0/70.
- GumbiousBishop $18 / $77DC6: incoming map165 uses Tree37 ($1F9580) entry33. Source takeoff and the full R36 world0 space pass precede B1 phase2's real MotaSpaceportBF / previousFFFF request at cells3C/26, facing0/alignment0. After reload, source music89 and production Character1 walking to1E0/170 run. Explicit Tree39 ($1FAAC0) entry17 executes its186/117/186 panel actions. One Torch8E is removed and Hydrofoil98 appended; the real map palette is restored before flagsHydrofoil9D/Hydrofoil2C0.
- B1 reuses existing continuation bytes and source map requests. No new RAM globals, actor-bank edits, accepted audio changes, automatic battle victories or invented story gates.

The source retail Torch reward scans beyond its40-slot inventory when full. R40 bounds the reward: a full inventory requires an explicit confirmed exchange of a sellable item. Cancellation and declined confirmation return to selection; zero-price key items are protected. No item is silently removed and no flag substitutes for the Torch. This overflow protection is a deliberate port behavior change. The ordinary Bishop route exchanges one existing Torch for Hydrofoil and stays within40 slots even when full.

## Verification

- LLVM18 native ARM build passes. Module24438920 bytes;726904-byte headroom in the24MiB native slot. Complete ROM33554432 bytes. First8MiB PSC shell, all27 accepted audio files and the full160480-byte/419-pose actor bank are byte-identical to R39. No asset regeneration or project hashes.
- IWRAM ends03006C90,112 bytes before IRQ reserve. EWRAM BSS ends02029FC5,59 bytes before presentation arena. EWRAM object names and sizes exactly match R39. Compiler static frames: new scenes24–40 bytes, continuation40 bytes, existing walking160 bytes. These are not a maximum call-chain stack proof.
- R31/R32/R34/R35/R36/R37/R38/R39 focused regressions pass, including599-frame crash,1638-frame flight,465-frame Dezo camera/snow,48-frame copied-Esper movement,480 independent walking frames and87 palette-reference VBlanks.
- R40 ASan/UBSan probe runs unchanged native scene headers with original dialogue/action bytes, production inventory/macro primitives, walking, source palettes and R36 space pass. Checks cover authored cue intervals, all F7 saved offsets, panel order, full300-frame source travel, map requests/continuations and flag order. Kyra is tested in every follower position1–4, with exact all64-byte carrier shifts, all8 macros and status clearing.
- Torch addition passes with each of40 free-slot positions and an adjacent-RAM sentinel. The full-inventory fixture exercises cancellation, protected key selection, cancelled/declined confirmation and explicit acceptance. Bishop exchange passes with Torch in each of40 occupied positions, preserves every other item and adjacent RAM, and runs the production map-palette helper before flags.
-18 original panels pass production Nemesis fallback, Enigma decoding, geometry, layer rendering and composition under ASan/UBSan. All691200 pixels match independent original-art decoding/tile sampling. Largest source-art contact sheet was visually inspected. No new panel assets were generated.
- Scene map loading, ordinary NPC wandering, tile animation, input/rendering and hardware are fixtures. Probe space art/decoding uses inherited source-data fixtures. Panel tests cover each panel separately and do not certify the live stacked UI lifecycle. No normal-save emulator/hardware full-game route or audio/frame timing test has run.
- Static dispatch coverage is156/161 ordinary events,25/33 real cutscenes,54/54 map-used updates,98/98 conditions and127/127 managers. Counts indicate native code paths, not complete behavior certification.

## Next work

Ordinary EclipseTorchStolen $46 and EclipseTorchUsed $47 remain missing. $46's flag gates the Air Castle route: R40's tested aftermath does not by itself connect that whole quest. Trace both original object controllers and art, implement the source choreography and flag order, and add independent state/clock probes. Do not bypass these gates. Other missing ordinary executors: MonsenEarthquake2C, MeetingLeRoof48 and DyingBoy86.

Missing scenes: AeroPrism1A, first Rykros arrival1B, LeRoofAgain1C, BeforeElsydeonCave1D, Elsydeon1E, Reunion1F, ProfoundDarkness20 and Ending21. Remaining composite/mechanical actors, movement-heavy combat controllers, Profound Darkness phases, ending/credits and a normal-save playthrough remain open. Preserve checkpoints while continuing.

Keep the accepted27-file audio, first8MiB PSC shell and current actor bank. No Butano, skipped frames, decoupled audio, cycle scaling, dropped source updates or flag bypasses. Authorized packaging is8MiB shell plus24MiB native module,32MiB total.

## Build and recovery

Working tree: /workspace/scratch/d7616bf13afc/r40_work/R40. Untouched verified baseline: /workspace/scratch/d7616bf13afc/r39_work/R39. Build with python source/build_r40.py; it reuses immutable assets with PS4_SKIP_ASSET_REGEN=1 and PS4_NO_HASH=1.

Compiler recovery: python source/tools/restore_clang18.py --out /absolute/path/compiler, optionally --archives /absolute/path/pinned-packages. Add compiler/bin to PATH and compiler/root/usr/lib/x86_64-linux-gnu to LD_LIBRARY_PATH. Current compiler: /workspace/scratch/d7616bf13afc/toolchain_download.

Run the source probes sequentially: check_r31_story.py, check_r32_vehicles.py, check_r34_scenes.py, check_r35_scenes.py, check_r36_spaceship.py, check_r37_scenes.py, check_r38_scenes.py, check_r39_scenes.py, check_r40_scenes.py and check_r40_panels.py. Then check_r40_preservation.py --baseline /absolute/path/R39/source and audit_r40_subsystems.py. Scripts are in source/tools.

Restore these exact archives in order. R40 final04 is cumulative against R39; R40 checkpoints01–03 are unnecessary for final recovery.

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
| PSIV_R40_CHECKPOINT_04_INCREMENT_2026-10-01.zip | Assigned after final recovery verification; external handoff is current. |

Run python source/tools/restore_r40_source.py --archives /absolute/path/archives --out /absolute/path/R40. It checks ZIP CRCs, strips revision roots and overlays one complete source directory. Source notes precede final saved-ID assignment; external R40_NOTES_2026-10-01.md contains final IDs and readback evidence.

PSIV_R40_ROM_2026-10-01.zip contains the complete32MiB ROM, requiring no source reconstruction to use. R39 fallback: PSIV_R39_ROM_2026-10-01.zip, libfile_f147e7a8b1ec8191a6aa168f707a9525, version0.

Intermediate checkpoint01: libfile_bce90c3f4e188191bd9a21fa1c2ae69c, version0,207735 bytes. Checkpoint02: libfile_a8be60ceb88481919b4c3ab1564630dc, version0,1119911 bytes. Both saved copies passed CRC and byte-exact readback. All new checkpoint ZIPs publish atomically only after closing and CRC verification. Final04 includes complete recovery evidence.

Checkpoint03: libfile_f74e3a0746808191934e38c78e365cfa, version0,1125059 bytes. Saved ROM: libfile_1b39e49106e08191bcffe4f73fd78f7d, version0,13595461 bytes; readback CRC and complete GBA bytes passed.

Final recovery: all ten archive CRCs pass; all932 current source files restore byte-exact. The complete recovered source built successfully and reproduced the24438920-byte module and complete33554432-byte ROM byte for byte. Final04 contains the recovery report; external handoff records final saved IDs/readbacks.
