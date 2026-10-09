# R31 Tonoe story checkpoint — 2026-09-30

Use RUN_THIS_R31_PSC_TONOE_STORY.gba. This continues the saved R30 checkpoint from the Build R17 PSC Updates chat. It preserves the accepted R28 audio, R29 presentation and R30 field-controller fixes. The interrupted chat's unpublished workspace was unavailable; this release was rebuilt from the saved R30 handoff.

## Changes

- RuneFlaeli ($27): party staging and dialogue resumes, Rune casting and flame actors, 14 source dust and 14 ballistic rock records, four original background chunk edits, camera/follower movement, cleanup and the TonoePathOpen flag.
- MeetingDorin ($32): original dialogue branch and music requests, Alys's wind-up and punch, anger marks, Dorin's impact/startle poses, actor hiding/restoration, source cue counters and completion flag.
- 54 immutable native GBA OBJ poses generated from the original Nemesis art and mappings. Tall flame poses use multiple hardware sprites; debris shares tiles. Actor state and 16.16 gravity use the existing source RAM records.
- Tile-cache invalidation handles overlapping allocations when temporary actors disappear or reappear. Custom sprites are cleared after the last temporary actor disappears. Maps without these actors skip the new rendering loop.

The first 8 MiB PSC launcher was compared directly with R30 and is byte-identical. The 27 retained audio implementation/bank files listed in R31_PRESERVATION.json compare directly with R30. Save layout and the four-game shell are unchanged. No Butano, frame skipping, audio decoupling, cycle scaling or omitted guest work was introduced.

## Verification and limits

The final ARM build passes the linker ROM/RAM boundaries. The 32 MiB ROM contains a 24,246,504-byte PSIV module with 919,320 bytes of slot space left. IWRAM has 336 bytes before the reserved IRQ stack; EWRAM BSS has 3 bytes before the existing presentation arena. R31 adds no ordinary EWRAM globals.

One focused ASan/UBSan host probe runs the actual new actor and event headers with the original PSIV ROM. It checks casting/explosion clocks, source state/flags, the completed-dialogue branch, punch cues, hiding/restoration, tile reuse and OBJ upload bounds. Movement, dialogue and display are stubbed; this is not an in-game scene test. The generated poses were also visually inspected. Leak scanning is disabled because this host cannot access its required process information; the probe makes no explicit heap allocations. No hashes or audio retuning were performed.

No normal-save emulator/hardware playthrough or hardware timing verification was completed. Later story, battle and ending work remains unfinished; the game is not certified finishable. Static dispatch coverage is 155/161 ordinary events, 8/33 cutscenes and 48/54 map-used updates. These counts identify code paths, not behavioral certification. Exact remaining IDs are in R31_SUBSYSTEM_AUDIT.md.

## Rebuild and next work

From source/, run:

    python3 build_r31.py

Use Python 3 and clang/clang++, ld.lld and llvm-objcopy on PATH; LLVM 18 was used. The alternative arm-none-eabi GCC toolchain path is inherited but was not exercised for this release. The included source ROM, donor shell, original disassembly and pre-generated assets make the handoff self-contained apart from the compiler toolchain. The build regenerates only the small R31 pose bank and the ordinary build inputs, retains the accepted asset banks, disables hashes and writes the ROM to ../binaries/.

When changing these controllers, run the single focused probe:

    python3 tools/check_r31_story.py

It requires host GCC with AddressSanitizer/UndefinedBehaviorSanitizer. Do not automatically run the historical broad suites or regenerate accepted audio. The coverage audit can be refreshed with python3 tools/audit_r31_subsystems.py.

Next verify Rune's Tonoe path and Dorin's meeting through a normal save. Then implement AlysWounded ($09), PsycoWand ($0A) and ZioDefeated ($0B) with their choreography/map continuations, plus PsycoWandChest ($2F) and its barrier/guardian actors. Teleport selectors, vehicle traversal/weapons, remaining battle movement/effect controllers, Profound Darkness phases and the ending remain. Preserve the existing audio and do not bypass flags to claim completion.

## Handoff contents

- binaries/: the R31 four-game ROM; donors/: the preserved 8 MiB PSC shell.
- source/: current runtime, headers, build scripts, generators, immutable banks, original PSIV ROM and original Mega Drive disassembly under references/disassembly/.
- source/docs/: these notes, exact build limits, subsystem audit and retained R30 baseline notes.
- build_artifacts/: build/validation logs, linker map, symbol/stack reports, a compiled R31 function listing, pose preview, source patch and preservation inventory.

The handoff excludes obsolete root release builders/tests, previous ROMs, the local probe executable, compiler object files and build caches. It retains the complete current runtime and the assets needed to rebuild.
