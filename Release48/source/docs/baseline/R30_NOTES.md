# R30 field-controller checkpoint

Use RUN_THIS_R30_PSC_FIELD_CONTROLLERS.gba. This builds on R29 and preserves the accepted R28 audio and R29 battle/dialogue presentation.

## Changes

- Fourteen original map palette controllers: Plate System, Nurvus, Wreckage engine, Garuberk, Rykros crystals, Le Roof and Vahal Fort. Authored flag gates, cadence and table offsets retained.
- Vahal Fort and Weapon Plant conveyor tile phases follow the terminal direction flags. Changed layout chunks are published through the existing dirty-chunk renderer.
- Strength/Courage ring chest completion flags and the Canceller gate now update.
- All six non-empty source tile-animation controllers run, covering 19 authored records in nine maps. State remains in the original E800 records; only changed tile frames upload.
- Field poison ticks every four walking tiles, can kill characters and end the game; paralysis recovery and Demi/Wren regeneration follow source cadence. Vehicles, idle frames, menus and cutscene waits do not heal the party. Each walking update is consumed once even across dialogue/event returns.
- Original Land Rover, Ice Digger and Hydrofoil field graphics/palettes, including the extra horizontal mapping pieces. Field vehicle animation uses the source five-update hold; vehicle encounters use the source 1/128 roll.
- AlysFound now has an explicit shared executor, preserves Chaz's position and adds Alys to macros. The trigger uses that same implementation.
- GettingLandRover now has source party destinations, slow convergence, dialogue resumes, the 360-update BG conveyor, field reload continuation, vehicle setup and ControlKey-to-LandRover inventory replacement.
- Corrected audit false positives: boarding, moving-platform/conveyor executors and the two guardian-defeat handlers were already present in R29.

## Validation and limitations

ARM build and slot/RAM/stack linker gates pass. One focused ASan/UBSan probe checks new field status and tile-animation clocks; all authored animation records fit the original slots. Accepted audio files compare directly, with no hashes or new audio testing.

The game is still not certified finishable. Eight ordinary event executors and 25 cutscenes remain missing; six map-used updates, teleport selectors, vehicle traversal/weapon parity and battle controllers still need work. Land Rover/story choreography and vehicle graphics are source-derived but have not been exercised through a normal emulator/hardware save. See R30_SUBSYSTEM_AUDIT.md for exact remaining IDs.

## Build / next agent

Run python3 build_r30.py with clang, ld.lld and llvm-objcopy available (LLVM 18 was used). It enforces PS4_SKIP_ASSET_REGEN=1 and PS4_NO_HASH=1, regenerates only the small R30 tables/vehicle poses and update continuations, then packs the unchanged PSC shell plus module.

Run python3 tools/check_r30_controllers.py only when changing the new controllers. It is the single focused probe; do not run the historical broad suites automatically.

First remaining story work: RuneFlaeli and MeetingDorin with their actual composite field actors, then AlysWounded, PsycoWand and ZioDefeated cutscenes with exact map continuations. Do not bypass flags or claim assets/debug fights supply the ending.

Changed source: gba/title_runtime_v06.c, the gba/r30_* headers, build_r30.py, tools/generate_r30_field_tables.py, tools/generate_r30_vehicle_field.py, tools/check_r30_controllers.py and tools/audit_r30_subsystems.py. Updated continuation data: gba/psc_runtime_plan.c and generated/psc_runtime_plan_report.json.
