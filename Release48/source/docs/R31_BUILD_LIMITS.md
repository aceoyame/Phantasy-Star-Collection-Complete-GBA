# R31 build limits

| Allocation | Final result | Remaining margin |
|---|---|---:|
| PSC PSIV module in 24 MiB slot | 24,246,504 bytes (0x0171F8E8) | 919,320 bytes |
| Combined four-game ROM | 33,554,432 bytes (32 MiB) | — |
| IWRAM allocation end | 0x03006BB0 | 336 bytes before IRQ stack bottom 0x03006D00 |
| EWRAM BSS end | 0x02029FFD | 3 bytes before presentation arena 0x0202A000 |
| Reserved IRQ stack | 0x03006D00–0x03007300 | 1,536 bytes reserved |
| Reserved System stack | 0x03007300–0x03007F00 | 3,072 bytes reserved |

The immutable R31 pose bank is 24,992 bytes with 54 entries. New actor state uses the existing C300+64-slot source RAM records and existing art-cache entries. No ordinary EWRAM globals were added.

The largest reported new function frame is r31_actor_draw at 304 bytes; RuneFlaeli reports 80 bytes and MeetingDorin 48 bytes. These per-function compiler reports are not a measured maximum call-chain or IRQ high-water mark. Linker boundaries pass; runtime stack/timing certification remains outstanding.

The accepted first 8 MiB PSC shell and 27 audio implementation/bank files compare directly with R30. The final ROM packer retains that shell. No hashes were calculated.

See the final R31_BUILD.log, R31_PACK.log, R31_SYMBOLS.txt, R31_STACK_USAGE.txt and R31_psc_module.map in build_artifacts/. Focused validation is recorded in R31_VALIDATION.log; no full playthrough was performed.
