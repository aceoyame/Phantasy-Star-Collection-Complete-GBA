# R38 — RajaSick and MeetingKyra

Final native build: `RUN_THIS_R38_PSC_RAJA_KYRA.gba`, full 32 MiB PSC packaging. Continues final R37. No emulator/hardware playthrough or finishability certification has been performed.

## Implemented

- Original RajaSick cutscene $12 / $7734C: incoming Tree18 ($1EC870) entries $59/$62/$63, initial NPC facing publication, eight Raja turns (one facing VB and ten map-only updates each), original collapsed Raja type$374 at source slot7, ChestOpened$E1, Suspicion$9F and two 20-update full-object waits. Original equal-distance facing leaves the old direction unchanged. Chaz/Rune/Rika/Wren face Raja before dialogue$62.
- Real flag$94 and Raja removal preserve native party-ID/carrier/macro compaction and clear his status byte. A real AF phase0 reload enters MeeseClinic F1$134, previous MeeseClinic$133, start($34,$38), face0/alignment0. Current-party slots1–4 receive the source positions and orientations. Tree18 entry$62 resumes at its actual F7 seam after map loading overwrites dialogue RAM.
- Clinic sequence restores the original Esper source mapping/palette fields; synchronizes the live native NPC state before copying; preserves the complete 64-byte carrier into source slot8; changes only the authored type$8194, steps, XY/destination and facing. Separate original Esper19E726 poses avoid reusing Saya's139FF8 art. The copied actor retains its palette and animation state and uses original 16-pixel step durations, ECE0 speed and source animation lists. Ordinary NPC wandering retains the existing native path; its full retail parity is not certified by this revision.
- Original X-then-Y, shortest12-bit FG/BG camera helper is reused for the clinic. Original Stairs$DF cue,20 map-only updates,48 full-object messenger updates, RedAlert$A9 and ten map-only updates remain. Entry$63 pauses at F7, all four characters turn down with one VB each, camera returns to Character1, dialogue resumes, and saved music becomes$A9. For the source-centered clinic fixture, the continuation has143 map updates and147 non-dialogue VBlanks.
- Original MeetingKyra cutscene$13 / $77788: panel$140, Tree37($1F9580) entry$27, source cutscene palette, Fal$92 before fade-in, panel$99 and entry$28 F7 seam,60-VB hold, panel$9B with120-VB hold, continued dialogue5 and all-panel destruction. Original text actions create$9A/$9C/$9D. Panel$140 uses the original ROM fallback above the precompiled panel bank's255-ID limit.
- Kyra enters party slot5, receives ATTACK in used macros and flag$A0. A real AF phase1 reload enters Dezolis1 / previousFFFF at source cells($174,$1C), face0/alignment0. LandMaster$8D, Character1 type$B4, cleared other party typewords, Vehicle_Index2, Map_Load_Flags bit0 and follower removal hand off to the existing native Ice Digger. Exact authored coordinates($BA0,$E0) are retained without the boarding event's coordinate snapping.
- AF uses the existing continuation bytes; no additional EWRAM globals. Automatic map music is suppressed during AF so source scene cues own both handoffs. R37 Landale, Dezo snow and Dark Force aftermath remain preserved.

## Verification

- ARM/LLVM18 native build passes: module 24,433,992 bytes, slot headroom 731,832 bytes, full PSC ROM 33,554,432 bytes. First 8 MiB PSC shell and all 27 accepted audio files are byte-identical to R37. No audio changes, blanket asset regeneration or project hashes.
- IWRAM end 03006C90 (112 bytes before IRQ reserve). EWRAM BSS end 02029FC5 (59 bytes before presentation arena). All EWRAM object names and sizes match R37. New scene stack frames are recorded in R38_STACK_USAGE.json.
- All 156,128 R37 actor-bank bytes are retained. Seventeen original-source poses append 4,352 bytes: collapsed Raja plus16 Esper walking/facing poses. Bank total 419 poses/160,480 bytes. The original-palette sprite preview was inspected; it is not an emulator screenshot.
- Existing R31/R32/R34/R35/R36/R37 probes pass, including vehicle terrain/speeds/ice edits, 599-frame crash,1,638-frame flight and465-frame Dezo camera/snow references.
- New ASan/UBSan scene probe compiles the actual R38 handlers unchanged with actual ROM dialogue and panel actions. Tests Raja in every follower position, carrier/macro compaction, dialogue seams across a real-map request, the exact 64-byte Esper copy, separate Saya/Esper poses, equal-distance facing, native NPC/source-record synchronization, Kyra slot/macro/flag and exact vehicle coordinates.
- Independent source FieldObj_GetAutoInput/Move/UpdateStepDuration/Animate2 reference matches all 48 copied-Esper position, step-duration, facing, frame, duration and palette records.
- The unchanged native high-ID panel fallback/compositor runs under ASan/UBSan with original decoded art/maps. All38,400 pixels match an independent source tile-sampling reference. Panel140 art is3,456 bytes within16,384-byte scratch; its maps are within2,048-cell scratch. Its preview was inspected.
- Map loading, ordinary NPC wandering, dialogue display cadence and hardware are stubbed in the scene probe. Panel test uses original Nemesis/Enigma output fixtures rather than exercising native decompression in that probe. No emulator/hardware save route or timing test has been performed. Static coverage is156/161 ordinary events,19/33 real cutscenes,54/54 map-used updates,98/98 conditions and127/127 managers. Counts mean native dispatch paths, not complete behavior certification.

## Next work

Continue with LutzRevelation$14 and MeetingSeth$15, then remaining story scenes and Rykros$1B. Ordinary events$2C/$46/$47/$48/$86, later composite/mechanical field objects, movement-heavy combat controllers, Profound Darkness and ending remain unresolved. Verify a normal save through Wren → sabotage → real Chaos Sorcerer → crash/Raja → Tyler/Hangar/Landale → real Dark Force1 → Zelan/Ice Digger → Raja's clinic → Kyra. Full game finishability remains unverified.

Keep persistent checkpoints as work progresses. Preserve accepted audio and PSC shell. No frame skips, audio decoupling, cycle scaling, dropped guest updates or story-flag bypasses. Current authorized packaging is8 MiB shell plus24 MiB module,32 MiB total. R37 remains the untouched fallback.

## Build and recovery

`python source/build_r38.py` regenerates only the small actor bank and reuses the large asset banks. Compiler recovery remains `python source/tools/restore_clang18.py --out /absolute/path/compiler`. Add compiler/bin to PATH and compiler/root/usr/lib/x86_64-linux-gnu to LD_LIBRARY_PATH. Current working compiler is `/workspace/scratch/d7616bf13afc/toolchain_download`.

Working tree: `/workspace/scratch/d7616bf13afc/r38_work/R38`. Previous verified tree: `/workspace/scratch/d7616bf13afc/r37_work/R37`. Final deliverable filenames: `PSIV_R38_ROM_2026-10-01.zip`, `PSIV_R38_CHECKPOINT_04_INCREMENT_2026-10-01.zip`, `R38_NOTES_2026-10-01.md`. The ROM ZIP contains the complete32 MiB GBA; no delta reconstruction is needed to play it.

Restore these exact saved archives in order, then overlay R38 final04. Final04 is cumulative against R37; R38 checkpoints01–03 are not required for restoration.

| Archive | Saved reference |
|---|---|
| PSIV_R32_CHECKPOINT_01_2026-09-30.zip | libfile_1470d4c0365c8191b65f8d032d37268f, version0 |
| PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip | libfile_25207882f74081918cfecd66ac195ef6, version0 |
| PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_1aa0718507488191ac101a8b148da823, version0 |
| PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_94852fb0d52c81918fe156a3ae0365b1, version1 |
| PSIV_R35_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_57a770bbb258819192aacc5b80e05679, version1 |
| PSIV_R36_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_63c577b95eb481919e7585f2995178eb, version0 |
| PSIV_R37_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_5954b2ae5c9481919801b58af04843b7, version0 |
| PSIV_R38_CHECKPOINT_04_INCREMENT_2026-10-01.zip | See final delivery references in the current external handoff |

Run `python source/tools/restore_r38_source.py --archives /absolute/path/archives --out /absolute/path/R38`. It checks ZIP CRCs, strips revision roots and overlays the complete source tree. R37/R38 linked maps, probe sources, evidence and previews are retained in build_artifacts. Source restoration and final file equality are checked before delivery.

R37 full ROM fallback: `PSIV_R37_ROM_2026-10-01.zip`, libfile_8c9e091af56c8191bd97cbc995749a17, version0. R38 source-archive notes precede assignment of final delivery IDs; the current external handoff includes those IDs.

Intermediate checkpoints saved as work progressed:01 libfile_5c6f6f1cd84c8191a915cbdefbea73a3;02 libfile_2d3ca45a76708191a94025ac443d5985. Final04 is sufficient for the R38 overlay.
