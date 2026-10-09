# R35 Wren and crash/Raja continuation

Baseline: final R34, preserved separately. R35 is an incremental native C GBA build, not a finishability-certified release.

## What changed

- MeetingWren ($0C): original opposite facing, panel $76 and 20-VBlank wait, current-map dialogue entry 1, Character_4 placement, party and ATTACK macro insertion, 32-byte NPC clear and WrenJoined ($70).
- CrashLanding ($0F): original manga/sound sequence, Tree14 entry 7, real Dezolis and Raja Temple map loads, source camera pan and snow effects, temple door animation, Tree14 entry 8 with all five F7 seams, preserved temple reload, full 64-byte Raja carrier transfer, ATTACK macro, final walk and RajaJoined ($88).
- Raja ($24) and snow ($1F4): original art/mappings lowered to immutable GBA OBJ poses. The 72,896-byte R34 prefix is retained; 31 poses and 34,816 bytes are appended. Total bank: 334 poses / 107,712 bytes.
- Off-screen actors continue simulating but do not allocate visible OBJ tiles. This prevents off-screen snow from occupying tiles needed by later visible actors.
- Crash FG/BG camera steps retain original 16.16 fractions and independent parallax. Presentation reuses existing rings and HBlank tables. Normal map music is suppressed during crash reloads because original RefreshMap keeps the script's music.
- The preserved temple reload snapshots all source/native objects in the inactive manga framebuffer during synchronous map loading. This buffer is not persistent scene state.
- No EWRAM object was added or resized. New references reordered compiler sections and initially added eight padding bytes. Sorting existing BSS sections by alignment reclaimed that padding; no stack/scratch boundary was moved.

Correct source IDs: $0C MeetingWren, $0D InsideSpaceship, $0E SpaceshipSabotage, $0F CrashLanding, $10 Landale, $11 DarkForce1Defeated. R34's future-work wording for $0D/$0E was incorrect.

## Validation

The final ARM build and focused ASan/UBSan host probes pass. The independent original-camera reference matches every one of the 599 crash frames and all 44 source snow launches. Tests check fractional motion/recycling, visible OBJ allocation on every pan frame, physical HBlank row values, original door records and six-update clocks, dialogue offsets across reloads, map handoffs, Wren/Raja macros and full Raja carrier copy. Prior R30–R34 scene, vehicle, travel, status and tile-animation probes pass.

These host probes use real ROM text/tables and unchanged scene/actor code. Hardware presentation, map asset loading, dialogue interaction and party walking orchestration are stubbed in the focused scene harness. No emulator or hardware playthrough has been performed. Static dispatch coverage is 156/161 ordinary events and 13/33 cutscenes; map-used updates, conditions and managers remain 54/54, 98/98 and 127/127. Counts are code paths, not gameplay certification.

All 27 accepted audio files and the first 8 MiB PSC launcher are byte-identical to R34. No blanket regeneration, hashes, frame skipping, audio decoupling, cycle scaling, story flag bypass or optional disassembly bugfix was introduced.

Final linked limits:

| Allocation | Result | Remaining |
|---|---:|---:|
| PSIV module in 24 MiB slot | 24,358,464 bytes | 807,360 bytes |
| IWRAM allocation end | 0x03006C88 | 120 bytes before reserved IRQ stack |
| EWRAM BSS end | 0x02029FC5 | 59 bytes before presentation arena |
| PSC ROM | 33,554,432 bytes | 32 MiB packaging |

The source/native reload snapshot uses 3,840 bytes of the existing inactive framebuffer. New compiled function frames are 32–96 bytes, excluding callees. This is not a measured hardware stack watermark.

## Next route blockers

Implement InsideSpaceship ($0D) and SpaceshipSabotage ($0E). They remain absent and block normal progression beyond Wren. Follow the original selector/name/mask records, cancellation behavior, source takeoff and landing loops, space pass, and Chaos Sorcerer battle 8 handoff; do not teleport directly or set crash/story flags to bypass them. CrashLanding is now dispatched, but normal-route arrival still depends on those handlers.

Shared source analysis:

- Destination names: ROM 0x2AAA7A, FE-delimited six worlds. Availability: priority records 0x63EF2, fallback 0x63EFE; sabotage has fixed mask 0x08 at 0x76587. Exclude current World_Index; store original ED40/ED42/ED43 chooser state.
- Surface/station takeoff lookup 0x64B02 (six 8-byte records); landing 0x64B34 and station entry/exit 0x64B5A (six 6-byte records each). Use real map continuations.
- Shared flight helpers: 0x641A2 hide/art; 0x64230 takeoff objects; 0x64332 landing objects; 0x643B8 space background; 0x644AE destination planet; 0x64C1E space pass.
- Surface takeoff 0x64568: up to 419 authored updates plus whitening/space pass; station takeoff 0x6488A: 228 updates. Original Speak skipping is allowed only where source permits. Landing deceleration is 257 updates if not skipped. All guest object/map/camera updates remain required.
- Sabotage: 112 source updates with literal fractional X steps and separate Y accelerations, original panel/text sequence, ChaosSorcr ($71), saved RedAlert music and battle 8; native battle result must drive crash continuation.
- Additional ship/jet, Landale-variant and planet source actors/poses are needed. Do not change the accepted audio implementation. Existing RAM is tight; prefer cold helpers, original RAM slots and existing temporary arenas.

Other ordinary blockers: $2C earthquake, $46 torch stolen, $47 torch used, $48 Le Roof, $86 dying boy. Later battle controllers, Profound Darkness, ending/credits and full normal-save/hardware validation also remain.

## Recovery and build

Checkpoint 04 is cumulative against final R34. Older R35 increments are not needed. Restore final R34 through its existing R32/R33 chain and R34 checkpoint 04, copy R34 to R35, then overlay R35 checkpoint 04. The checkpoint contains changed source, pose bank, generated reports and focused evidence. Keep the accepted shell/audio baseline.

From R35/source, run `python3 build_r35.py` with the existing LLVM ARM toolchain available in PATH and its runtime libraries available. The wrapper regenerates only the small field pose bank, sets PS4_SKIP_ASSET_REGEN=1 and PS4_NO_HASH=1, and packs the unchanged 8 MiB shell with the native module. Output: RUN_THIS_R35_PSC_WREN_CRASH.gba.

Focused reproduction: check_r31_story.py, check_r34_scenes.py, then check_r35_scenes.py. Use check_r35_preservation.py --baseline /absolute/path/to/R34/source for byte comparisons and RAM/ROM bounds. The predecessor linked map and R34 preservation report must be available for the allocation comparison.

## Saved ROM reconstruction

The full R35 ROM archive save was rejected because the storage limit was reached. The 82,405-byte PSIV_R35_ROM_RECOVERY_2026-09-30.zip instead preserves the exact ROM using the already saved R34 archive. Reconstruction is verified byte for byte against the final 32 MiB R35 build; it uses only Python's standard library and no digests.

Unzip the recovery package and run `python3 restore_r35.py /path/to/PSIV_R34_ROM_2026-09-30.zip`. It writes RUN_THIS_R35_PSC_WREN_CRASH.gba. You may also supply the unzipped R34 GBA ROM. The package includes baseline byte checks and complete instructions in README.txt. No compiler is required for this restoration.

Persistent R34 ROM baseline: PSIV_R34_ROM_2026-09-30.zip, libfile_6582de37c7208191a6dd28c31928dddb. This is distinct from the source checkpoint chain; the ROM recovery package needs only the R34 ROM, not R32/R33 source archives.
