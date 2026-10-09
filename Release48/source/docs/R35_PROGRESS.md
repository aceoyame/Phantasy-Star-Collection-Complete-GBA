# R35 recovery status

Baseline: final R34, retained separately. Work in progress.

Authorized goal: continue toward normal play with source-derived Wren and spaceship route. Source table correction: $0C=MeetingWren, $0D=InsideSpaceship, $0E=SpaceshipSabotage, $0F=CrashLanding. Earlier handoff wording of FallingSatellite $0D / CrashLanding $0E was inaccurate.

Preserve 27 accepted audio files, the first 8 MiB PSC shell, and all R34 actor poses. Use no story flag bypasses or new EWRAM globals. Source script and motion/controller analysis in progress; no R35 build yet.

Checkpoint 01: design and corrected source IDs saved persistently.

Implementation stage: MeetingWren ($0C) and CrashLanding ($0F), the snow/Raja actors, original camera acceleration and BG parallax, temple door records, preserved map reload and Raja join are wired into the native dispatcher. Initial ARM build in progress; these changes are not yet validated. InsideSpaceship ($0D) and SpaceshipSabotage ($0E) remain unimplemented and are the next route blockers. This release will not claim those two handlers or normal-route completion.

Validation stage: ARM build and focused R35 scene probe passed. Independent ROM-derived reference matches all 599 camera frames and 44 snow launches. Real-ROM Raja text has five F7 seams, preserved across the temple reload; original door records hold for six map updates each. Every visible Raja/snow actor fits the OBJ tile/OAM budget during the tested pan. Prior R30–R34 probes pass. Full map objects/carriers survive the same-map snapshot.

RAM: no EWRAM object was added or resized. Existing BSS sections changed order when referenced by the new code, adding eight padding bytes and initially exceeding the scratch boundary by one byte. Sorting those existing zero-initialized sections by alignment restores 59 bytes of headroom. IWRAM remains 120 bytes before the reserved IRQ stack. Snapshot borrows the inactive manga framebuffer synchronously; no persistent state resides there. R35 continuations suppress automatic map music because original RefreshMap preserves the authored crash music. Final rebuild/preservation check and delivery pending. No emulator/hardware playthrough has been performed.

Final ARM rebuild/preservation check passed: module 24,358,464 bytes, slot headroom 807,360 bytes; ROM 32 MiB. Final name RUN_THIS_R35_PSC_WREN_CRASH.gba. Current coverage 13/33 cutscenes; InsideSpaceship and SpaceshipSabotage remain absent. Full ROM archive and final cumulative checkpoint are being saved. See R35_NOTES.md for tested scope and next exact source addresses.

Delivery: full R35 ZIP save rejected by storage limit. Compact exact ROM reconstruction succeeded: 82,405 bytes, 968 copy records / 123,501 literal bytes; output compared byte for byte with final R35. The package needs only the saved R34 ROM ZIP and Python standard library. Final source checkpoint 04 includes the recovery generator/scripts and validation report. Full build can also be regenerated from source.
