# R37 — Landale, Dezolis spaceport and Dark Force 1 aftermath

Final native build: `RUN_THIS_R37_PSC_LANDALE.gba`, full 32 MiB PSC packaging. Continues final R36. No emulator/hardware playthrough or finishability certification has been performed.

## Implemented

- Original Landale cutscene $10 / $77150: source panel $8B and actual Tree14 ($1E99F0) entry $24 with $8C/$8D actions. Dialogue5 closes with fade-out and all-panel destruction. A real map continuation reloads Dezolis 1 with previous Hangar $15F at ($12,$92), facing8 / alignment$C, music$99, then runs the spaceport sequence before flag$82.
- The original Event_DezoSpaceportAppearing $11 now replaces the simplified earlier native Dezolis effect. Original three Nemesis streams occupy source RAM $0000/$0800/$1000. Snow dividing $78, lifted snow $7C and spaceport reveal $84 use the original 16.16 coordinates/velocity, durations, life counters and mappings, correct priority and palette. X then Y camera motion uses the shortest wrapped 12-bit path and updates FG/BG together.
- The rising sequence publishes each frame before its source table launches. The $170 DBF loop runs 369 updates. All 24 table rows are retained; first-match behavior makes two duplicate-timer rows unreachable, so 22 particles launch. Original $F8 divider/reveal creation, BG (6,$24) chunk $2C replacement at $3D, 47 grave-opening cues and return to Character1 remain. For the normal authored entry, outbound/return camera travel adds 96 updates, totaling 465.
- Original DarkForce1Defeated $11 / $771D2: panel$8E, actual Tree35 ($1F81B0) entry7 and $8F/$180/$90/$91 actions; two 60-update map waits around music$9B. Clears exactly original $C000..$CFFF object memory, preserves party IDs/inventory, sets world3 and runs the existing R36 source space pass.
- A real map continuation enters Zelan F1 $18E / previous$FFFF at ($3E,$20), facing0 / alignment8. Music$89, panels$92/$93, 60/120 waits, special radar cue$F8, original palette table$77332 (4,4,60,4,4,120 VBlanks; 196 total), source stop-special-SFX command$FD, and actual Tree35 entry8 with original panel/music actions remain.
- Removes exactly one Canceller$9A and compacts inventory, then appends IceDigger$97 before setting flag$89. The normal source route owns a Canceller, so even a full 40-slot inventory has room after this exchange. No fake victory, forced battle or invented story gate. Exceptional corrupted saves with a full inventory and no Canceller are not covered by this normal-route guarantee.
- AE uses the existing native map continuation bytes. New scenes stop the field HBlank publisher before bitmap reuse and rebuild the field at return. Map-load automatic music is suppressed during AE so source scene music writes own the handoff.

## Verification

- ARM/LLVM18 build passes. Native module 24,424,224 bytes, native slot free 741,600 bytes, complete PSC ROM 33,554,432 bytes.
- All 27 accepted audio files and first 8 MiB PSC shell compare byte for byte with R36. No audio changes, blanket asset regeneration or project hashes.
- All 148,960 R36 actor-bank bytes are retained. Fourteen new poses append 7,168 bytes; total 402 poses / 156,128 bytes. Original palette sprite review PNG was inspected; it is not an emulator screenshot. Earlier Motavia place effects keep their accepted path.
- IWRAM end 03006C88 (120 bytes before IRQ reserve); EWRAM BSS end 02029FC5 (59 bytes before presentation arena). All EWRAM object names and sizes match R36. New scene stack frames are recorded in R37_STACK_USAGE.json.
- Existing R31, R34, R35 and R36 probes pass, including the 599-frame crash and 1,638-frame ascent references.
- New ASan/UBSan host probe compiles the R37 handlers unchanged with actual source text/art. It checks real map requests, text/panel/music actions, source radar colors/holds/cues, source object-memory range, genuine inventory helpers with full and duplicate-Canceller inventories, flags and OBJ bounds/priorities.
- An independent instruction reference matches every camera and snow actor position, life counter, animation frame/duration and base-tile value across all 465 normal-entry updates and 22 first-match launches. Map-used updates/conditions/managers remain covered.
- Hardware DMA, real map loading, dialogue display, panel drawing and combat are stubbed in host probes. Dialogue display cadence and the normal-save battle/story route still need emulator/hardware verification. Static dispatch coverage is 156/161 ordinary events and 17/33 real cutscenes; counts indicate code paths, not complete choreography certification.

## Next blockers

Continue with RajaSick $12 and MeetingKyra $13. Original Rykros $1B, later scenes/controllers, Profound Darkness and ending remain blocked. Ordinary events $2C/$46/$47/$48/$86 remain open. Verify a normal save through Wren → sabotage → real Chaos Sorcerer → crash/Raja → Tyler/Hangar/Landale → real Dark Force 1 → Zelan/Ice Digger. Full game finishability remains unverified.

Keep persistent checkpoints. Preserve accepted audio and PSC shell. No frame skipping, audio decoupling, cycle scaling, dropped guest updates or story-flag bypasses.

## Build and recovery

`python source/build_r37.py` regenerates only the small actor pose bank; all large asset banks are reused. Compiler recovery remains `python source/tools/restore_clang18.py --out /absolute/path/compiler`. Add that compiler's bin directory to PATH and root/usr/lib/x86_64-linux-gnu to LD_LIBRARY_PATH.

Restore the exact saved archives below in order, then overlay final R37 checkpoint04. R37 final04 is cumulative against R36; R37 checkpoints01–03 are not required for restoration.

| Archive | Saved reference |
|---|---|
| PSIV_R32_CHECKPOINT_01_2026-09-30.zip | libfile_1470d4c0365c8191b65f8d032d37268f |
| PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip | libfile_25207882f74081918cfecd66ac195ef6 |
| PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_1aa0718507488191ac101a8b148da823 |
| PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_94852fb0d52c81918fe156a3ae0365b1 (version 1) |
| PSIV_R35_CHECKPOINT_04_INCREMENT_2026-09-30.zip | libfile_57a770bbb258819192aacc5b80e05679 (version 1) |
| PSIV_R36_CHECKPOINT_04_INCREMENT_2026-10-01.zip | libfile_63c577b95eb481919e7585f2995178eb (version 0) |

Run `python source/tools/restore_r37_source.py --archives /absolute/path/archives --out /absolute/path/R37`. It checks ZIP CRCs, strips revision roots and overlays into a complete source tree. R36/R37 linked maps and source-probe evidence are retained in build_artifacts. Final R37 source restoration is checked against the actual checkpoint files.

R36 full ROM remains PSIV_R36_ROM_2026-10-01.zip, libfile_04e754393bdc81919611dc751f681800. The R37 delivery is a complete ROM ZIP and requires no delta reconstruction.
