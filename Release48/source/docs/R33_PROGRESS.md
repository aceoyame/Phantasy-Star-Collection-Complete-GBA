# R33 recovery checkpoint

R32 remains intact. R33 continues the source-derived native C GBA port.

Checkpoint 04: R33 final source and verified ARM build. See R33_NOTES.md for complete recovery and remaining-work details.
The chest barrier, 18 splinter facings and Gy-Laguiah mappings have been lowered
offline into 200 additional immutable native poses. Original R32 pose prefix
remains retained. Native source controllers and Event_PsycoWandChest ($2F) dispatch are installed.
Sanitized host checks passed for flicker boundaries, 11-update barrier break,
39-update fragment lifetime and exact 16-byte source helper stride,
244-update guardian reveal, dialogue continuation and event battle $05 handoff.
Module is 24,306,016 bytes, leaving 859,808 bytes in the 24 MiB slot.
AlysWounded ($09) now dispatches with both native reload continuations,
the optional Saya healing visit, source Tree6 text seams, party and macro
compaction, Demi joining and restored Tree5. Source Saya walking, healing stars
and Alys-in-bed graphics/controllers are appended to the immutable actor bank.
The Alys host probe passes both story branches and verifies outgoing equipment
is retained; movement, dialogue display, fades and battle are stubbed in probes.
No emulator or hardware verification has been performed.

Next: verify preserved shell/audio and regression probes, then save R33 ROM.
PsycoWand ($0A) and ZioDefeated ($0B) remain separate cutscene blockers.

Build: set PATH to the recovered LLVM bin and LD_LIBRARY_PATH to its
root/usr/lib/x86_64-linux-gnu, then run python3 build_r33.py in R33/source.
Use --baseline <R32/source> with tools/checkpoint_r33.py for cumulative updates.
Every increment overlays the final R32 source and does not require prior R33 increments.
Accepted audio and the first 8 MiB PSC launcher have not been changed.
No emulator or hardware playthrough has been run.
