# CP58 — native text and decoder kernels

Authoritative continuation of CP57 (CRC32 4CA93430). Final ROM: `RUN_THIS_R48_CP58_NATIVE_TEXT_KERNELS.gba`.
Size: 33,554,432 bytes. CRC32: **63CE7F96**. SHA256: `987accb2e626c4bcc3a0c6af15aadada6a2499cb2117508f7a093d4ce1ff5653`.

## Result
The minimum across the same 24 matched measurement windows rises from **40.5730% to 42.5848%** of full speed, a **4.958%** floor throughput gain. The minimum returns to Anne's 450–750 help-screen window. That original CP56 37.3257% window is now **42.5848%**. Anne's opening window rises from 40.5730% to **43.5512%**. Across all measured windows, throughput rises **2.5236%** over CP57.

This remains below full speed. Results are actual ARM7 execution in mGBA under a fixed input script, not physical GBA timings or a guarantee about every scene. Seven later windows have small regressions, at most **0.2151%** additional cycles; all opening and middle windows improve. Avoid claiming that all 24 windows are faster.

## Accepted implementation
1. Shared text output: one native Thumb function (aotf597) implements the byte-identical 50-byte, 19-instruction routine at seven relocated adventure addresses. 133 entry maps are enabled. The original character reads, newline handling, glyph call, VRAM writes, stack restore and full CCR remain intact. Relative entry offsets reconstruct each game's PC/return address. Nei differs and retains its existing path. The bitmap pool remains 34 live rows.
2. Shared checksum: six originally identical AOT bodies (86/87/88/89/90/92) share one implementation through function aliases. md_native_checksum batches only complete 16-cycle ADD.L/DBRA iterations over aligned ordinary ROM/RAM. ROM batching stops at a 4 KiB page/end boundary; RAM at its 64 KiB boundary. Partial budgets, halfword alignment, wrap and MMIO retain the original reads. The final addition restores every CCR bit, including overflow. No writes/events are elided. Alias reuse recovers main ROM space.
3. Packed-nibble decoder: md_native_repeat_nibble batches complete 30-cycle iterations of aotf33's register-only repeat loop. It stops before the zero-count flush branch or the current cycle limit. Eight or more repeats have an exact closed form; shorter runs execute the same recurrence. Terminal counter, full CCR, signed-overflow and partial-instruction boundaries are preserved.
4. Cold shift placement: do_shift moves from IWRAM into ordinary ROM, freeing 424 bytes. The body and ABI are retained. It is rare in measured profiles; this trades a small later-window cost for room to speed opaque text rendering.
5. Renderer: a guarded ARM single320_opaque emitter packs the exact six source pixels used for each 8-pixel cell into three pixel pairs. Eligibility requires the existing full-width, opaque, high-priority window proof and absence of overlapping sprites. The generic compositor resolves high-priority sprite/window pixels before decoding plane B. Transparency, mixed priority, sprites and other modes keep their existing composition rules.

The sound sources, wave bank, guest dispatcher, scheduler, PS1/Gaiden/SMS-GG engine, PSIII panel and upper original content bank are preserved. No sound quality, frame budget, stack boundary or memory cap was relaxed.

## Measurements
Eight games × 1,050 guest frames; identical scripted inputs and audio calls. Windows are 300 guest frames. 100% means one guest frame per GBA display frame. Overall cycles saved: 105,591,043/4,289,690,887 (2.4615%).

| Game | 150–450 | 450–750 | 750–1050 |
|---|---:|---:|---:|
| Anne | 43.5512% | 42.5848% | 48.8705% |
| Rudger | 44.0932% | 49.1279% | 56.3166% |
| Kinds | 45.4797% | 52.7241% | 60.4065% |
| Shilka | 46.5838% | 52.3531% | 48.9774% |
| Amia | 44.3159% | 56.4812% | 42.6902% |
| Huey | 44.8339% | 48.1827% | 54.0596% |
| Eusis | 44.7602% | 46.6458% | 48.2858% |
| Nei | 47.8971% | 53.7534% | 47.3846% |

## Verification
- All eight adventures: exact captured CPU/game/memory state, pixels/palette and stereo PCM versus CP57.
- Shared text output: 14,896 differential cases, all 133 relocated entry maps, partial budgets, cycle wrap, newline/NUL/byte values, RAM/ROM/wrap operands, VRAM writes and stack overlap.
- Checksum: 37,632 differential cases, seven game maps, both entry phases, ROM page/end, RAM wrap/alignment, MMIO fallback, carry/overflow/sign/zero and cycle wrap.
- Packed-nibble loop: 40,768 differential cases, all 91 common-game entry maps, repeat/flush counts, decoder bytes, full CCR and partial cycle budgets. Each compares the complete function against CP57's original AOT.
- 3,600 actual ARM calls verify relocated do_shift's output, full CCR and stack ABI.
- 22,528 original CP07 renderer comparisons include window modes, priorities, transparency, flips, scroll, sprites and both framebuffer pages; 672 directed lines exercise the opaque cover guard.
- All eight EEPROM save slots preserve a non-default saved word through power-cycle/reload and leave unrelated save regions untouched.
- Full canonical build ELF is byte-identical to the measured ELF. Candidate/release ROM differences are confined to unused section tails. Preserved ROM banks are byte-identical to CP57.

## Space and report correction
IWRAM end 030078AC: **212 bytes spare**.
EWRAM end 0203DDE4: 540 bytes spare.
Main ROM end 0872FB10: 1264 bytes spare.
ZAOT spare 492; AOTMORE spare 156; AOTEXTRA spare 104; ROM2 spare 2696.

CP57's packaging report misread the first linker-map column for __rom_end and reported an EWRAM address as the main ROM end. Its actual main ROM end was 0872FF24, only 220 bytes spare. CP58 reads the absolute symbol from llvm-nm. This corrects reporting; actual linked blobs and ROM section caps were enforced throughout.

## Rejected experiments
- Packing dispatcher cache index/round-robin into one word gave inconsistent gains and worsened the target window. Final aotdisp.c is byte-identical to CP57.
- Tracking opaque windows to suppress background-only dirty lines passed host renderer comparisons but slowed the target ARM measurement. It is rejected. The final renderer has no hidden_window_lines/window_tiles metadata. Rejected source, checks and stage reports are retained only as research.

## Continue from here
The slow help-screen window remains dominated by graphics. Its final profile begins: render_line 14.83%, ch_render 9.33%, snd_mix2 6.53%, m68k_exec 4.90%, z80drv_run 4.79%, plane_cells 3.26%, aotf70 3.17%, single320 2.49%, prepare_plane_row 2.48%, snd_mixn 2.32%. Background dirty suppression did not pay for its bookkeeping. Profile the actual remaining generic render_line paths, tile fetching and clipping before adding more metadata. Opening-window checksum and repeat-decoder loops are now native transactions; more gains require other measured guest leaves or less host compositor work. Existing PS4 speed is not evidence these Genesis CPU/VDP paths have the same costs.

Final source is authoritative. Do not rerun exploratory installers over it. `native58/measure_batch.py` uses CP57/cp57_base as reference and native58/internal_candidate.gba with native58/armcheck/md.elf as candidate. Clear every candidate capture after any change; CP46_REUSE otherwise silently reuses previous results. Always stop after a link failure. The complete checkpoint omits raw state/pixels/PCM captures. Recreate the internal candidate with `python3 native58/link_candidate.py` using the included final objects, then regenerate captures. CP46_REUSE requires a complete set of capture files and JSON; incomplete cached runs are automatically regenerated.

Rebuild offline after extraction:
```bash
tar -xzf SUPPORT/R48_LLVM18_1_3_MINIMAL_REBUILD_TOOLCHAIN.tar.gz -C Release48
python3 Release48/source/build_cp58.py --out RUN_THIS_R48_CP58_NATIVE_TEXT_KERNELS.gba
```
For host ARM measurements, build mGBA with source/tools_host/r46_mgba/build_minimal.py and source/tools_host/cp08_mgba/build_probe2.py. Set LLVM PATH/LD_LIBRARY_PATH and MD_ELF to the matching ELF. Included source snapshots, reference ROM/ELF, tests, profiles, compiler archive and final objects permit continuation without earlier chat state. Source delta applies only to CP57; the complete checkpoint is self-contained.

SHA256 was computed once for the final ROM only.
