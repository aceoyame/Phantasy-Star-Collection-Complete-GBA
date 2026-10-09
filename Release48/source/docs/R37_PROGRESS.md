# R37 checkpoint 01 — intact R36 and original source trace

R36 final is preserved in r36_work/R36. R37 continues only Landale $10 and DarkForce1Defeated $11. No R37 ARM build yet.

- Landale: original panel $8B, Tree14 entry $24 with panels $8C/$8D, real Dezolis map 1 / previous Hangar $15F at ($12,$92), facing8 align$C; music $99; Event_DezoSpaceportAppearing $11 (existing native source family) before flag $82.
- DarkForce1Defeated: original Tree35 entry7/panel$8E; 60 map updates twice around music$9B; clear original 4KiB custom objects; world3 and existing R36 300-update space pass; real Zelan F1 $18E / previous$FFFF at ($3E,$20), facing0 align8; music$89; panels$92/$93 with 60/120 VBlanks; literal radar table$77332 and cue$F8/$FD; Tree35 entry8; remove one Canceller$9A, compact inventory, add IceDigger$97, flag$89.
- Dialogue5 ends with fade-out and destruction of all panels. No F7 seams in these three source entries.

Keep source battle/story flags as authored. Preserve accepted 27 audio files, PSC shell, native RAM allocations and all R36 actor poses. No frame skips or blanket regeneration. Continue checkpointing as work advances. Emulator/hardware playthrough remains unverified.

Checkpoint 02: Landale/DarkForce handlers and AE dispatch are implemented, plus Dezolis-only $78/$7C/$84 actors and 14 appended source poses (7,168 bytes; R36 prefix retained). Source place loop preserves first-match table behavior (two duplicate timer rows are deliberately unreachable), launches after VB, retains 16.16 velocity and X/Y wrapped camera travel. Probes/build pending.

Checkpoint 03: ARM build and all R31/R34/R35/R36 regressions pass. New ASan/UBSan scene probe passes actual-ROM panel/dialogue actions, real map requests, 196-VBlank radar sequence, one-Canceller full-inventory Ice Digger exchange, and unchanged earlier Motavia effect classification. Independent instruction reference matches every one of 465 camera/effect frames and 22 first-match particle launches. Linked RAM boundaries and all EWRAM object sizes match R36. Final notes/archive/ROM packaging pending. No emulator/hardware playthrough.

Final checkpoint04: source/ARM/probe/preservation checks passed. The complete seven-archive source chain was restored and every final increment file and PSC donor compared byte for byte. Complete R37 ROM and cumulative source checkpoint are the final deliverables. R37_NOTES.md is the authoritative handoff.
