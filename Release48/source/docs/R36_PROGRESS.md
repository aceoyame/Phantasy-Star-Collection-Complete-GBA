# R36 checkpoint 01 — recovered R35, spaceship route mapped

Restored R35 from the complete saved R32→R35 source chain; restored and saved the full R35 ROM as PSIV_R35_ROM_2026-10-01.zip (libfile_54b8e31ef06c8191a95f9789b0c2edaf).

R36 targets original Cutscene_InsideSpaceship $0D and Cutscene_SpaceshipSabotage $0E, using source destination masks/names, native takeoff/space-pass/landing, required ship/planet actors, and the genuine battle 8 result path into R35 crash landing. No gameplay implementation changed at this checkpoint.

Preserve accepted 27 audio files, first 8 MiB PSC shell, 32 MiB packaging, all source updates and 16.16 camera clocks. No blanket asset regeneration. No new EWRAM globals; IWRAM has only 120 bytes spare. Existing source RAM and presentation scratch can serve synchronous flight presentation. Source addresses: selector $63BC4, takeoff $64568/$6488A, destination tables $64B02/$64B34/$64B5A, space pass $64C1E, sabotage $76060-$7658A.

Still required: implement, probe source state/timing and cancellation, compile ARM, compare accepted bytes and RAM, save ROM and final cumulative checkpoint. No emulator/hardware playthrough claimed.

## Checkpoint 02 — native implementation, verification pending

Added source destination masks/names and solar-system tile/palette selector, real-map AD continuations, full ascent/landing clocks, fixed-point camera wrapping, source space pass and planet mapping, ship/Landale actor counters and append-only bank. Added sabotage panels/Tree35 F7 continuation and genuine battle 8; existing RunEvent $29/flag $71 requests crash afterward. No ARM validation yet. First Rykros arrival remains blocked by source cutscene $1B, not skipped. Accepted audio and shell untouched.

## Checkpoint 03 — ARM build and focused source checks passed

The R36 spaceship probe passes ASan/UBSan with real source art/tables and Tree35 text. Hardware DMA, actual map loading and battle combat are stubbed. An independent instruction reference matches 1,638 frames across both ship variants and all ascent modes (including whiteout). Original selector mask priority and bit4 Cancel, 12 space-pass art cases, source actor timers, landing camera/deceleration, all real map requests and the battle8 handoff pass. Existing R31, R34 and R35 regression probes pass. R36 ARM build is 24,411,968 bytes, 753,856 bytes free in the native slot. IWRAM/EWRAM boundaries and object sizes are unchanged; all 27 accepted audio files, first 8 MiB PSC shell and 107,712-byte R35 actor bank prefix compare exactly.

Next: final review/coverage notes, preserve linked map and compiler recovery script, save complete ROM and cumulative checkpoint. First Rykros arrival ($1B) and later blockers remain unresolved; no playthrough claim.

## Final checkpoint 04 — delivery ready

Final module is 24,413,216 bytes, with 752,608 bytes free. All focused route/reference, existing regressions and preservation checks pass. Display ownership is fixed; source stars/ship cache, source caption timing and initial space fade are retained. Complete linked maps, stack evidence, compiler recovery and source restoration scripts are preserved. Deliver the full ROM ZIP plus this cumulative source checkpoint and R36_NOTES.md. R36 checkpoints 01–03 are already saved; older R36 increments are not required to restore final R36. Read final notes for the exact earlier archive chain. No normal-save emulator/hardware playthrough has been completed.
