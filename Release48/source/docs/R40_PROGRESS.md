# R40 progress

Continues verified R39. R40 implementation checkpoint01 adds LashiecDefeated $16, DarkForce2Defeated $17, and GumbiousBishop $18. Incoming Lashiec/DF2 dialogue is Tree22. Bishop incoming map165 uses Tree37 (not Tree38 or22). Existing R36 space pass carries world1/0 travel; B1 continuations reload Gumbious162, Dezolis1, MotaSpaceportBF. Source cues, F7 seams, batch panel pops, Kyra removal, torch recovery and Hydrofoil exchange are connected.

Normal Torch reward uses existing inventory add. Retail scans beyond its40-slot inventory when full; native full case instead requires an explicit confirmed exchange of a sellable item, protecting key items and adjacent RAM. This overflow protection is a documented port behavior change, not a story-flag substitute.

No new art/audio/RAM globals. Build, probe and restoration verification pending. This implementation checkpoint is not a validated playable ROM.

Checkpoint02: LLVM18 native build passes. Module24438920 bytes,726904-byte slot headroom, full32MiB ROM. All27 accepted audio files, first8MiB shell, entire160480-byte actor bank and EWRAM object sizes match R39. IWRAM03006C90 (112-byte reserve margin), EWRAM02029FC5 (59-byte presentation margin). R31/R32/R34/R35/R36/R37/R38/R39 regressions pass.

R40 ASan/UBSan source probe passes actual Tree22 Lashiec/DF2 seams/actions/cue sequence and authored holds, full300-frame source space passes, real map requests/continuations, Kyra slots1..4 exact carrier/macro/status compaction, Torch rewards in all40 free slots, confirmed full-inventory exchange with cancellation/key protection, and Hydrofoil exchange with Torch in all40 occupied slots. Production party walking and map-palette restoration execute. Map loader, ordinary NPCs, UI/hardware are fixtures.

18 panels decode through production Nemesis/Enigma and unchanged fallback/compositor; all691200 pixels match independent source decoding/tile sampling. Contact sheet inspected. No normal-save/hardware full-game route or live inventory-screen test has been performed.

Checkpoint01/02 saved copies passed byte-exact readback and CRC. Final ROM packaging and full ten-archive recovery verification are in progress. Next ordinary blockers are Torch theft46/use47; theft flag gates Air Castle. No bypass was added.

Final recovery: all ten archive CRCs pass; all932 current source files restore byte-exact. The complete recovered source built successfully and reproduced the24438920-byte module and complete33554432-byte ROM byte for byte. Final04 contains the recovery report; external handoff records final saved IDs/readbacks.
