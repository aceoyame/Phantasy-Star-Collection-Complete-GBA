# Stage 17 — OBJ mutation recorder, live-core synthetic smoke

Date: 2026-11-23. **Experimental source/tooling only; no cartridge graphics modified.**

## New contribution

Stage16 demonstrated that OAM tile indices 448, 480, and 488 are mutable VRAM slots, not stable character/pose IDs. The new source-only [trace_stage17_obj_uploads.py](trace_stage17_obj_uploads.py) samples the **entire 32 KiB OBJ VRAM**, 1 KiB OAM, sprite palette and framebuffer at each emulated frame, alongside map ID, player world coordinates, raw actor record and native 1D/2D OBJ geometry.

The trace reports all changed 32-byte tile regions, before/after SHA256, changed byte counts, OAM/palette changes, visible sprite hashes, and screenshot CRC32. It handles 4bpp and 8bpp sprites and retains native 16x32 geometry without rescaling. The script hard-fails on an incorrect ROM SHA256, malformed geometry, oversized route, unexpected map ID or non-32-MiB PSC input. It never changes the game ROM or game memory.

These observations prove **between-frame changed bytes**, not specific CPU store instructions, DMA events, compressed Genesis source pointers or actor identity. The optional existing mGBA bridge visible-scanline CPU write counter excludes DMA and VBlank. Do not relabel this timeline as a true bus-write provenance trace.

## Executed this run

- Inspected actual Library archives: PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16.zip (mGBA 0.10.5 bridge, probe.c, probe.so, libmgba.so, mgba2.py) and R234_SOURCE_TOOLS_EVIDENCE.zip.
- Three local unit tests passed: mutable/reused OBJ slot, 16x32 4bpp and 16x16 8bpp sprite geometry, and invalid controller-route rejection. Committed [test_stage17_obj_uploads.py](test_stage17_obj_uploads.py).
- Built an **original, temporary synthetic GBA test program**, not a copyrighted commercial cartridge. It writes a known 32-bit word to OBJ VRAM at 0x06010000 and initializes OAM. Executed **two actual frames** on the archived native headless mGBA bridge. The tracer detected **one mutation frame**, changed OBJ tile index 0, **four changed bytes**, an OAM change and active 1D OBJ mapping. SHA256 of this synthetic test binary: 2d2012a141f70013b6868774196df60a0d7ea99a35d41f2f47807e01012fdaa6. This is an **emulator-tool integration smoke only**, NOT a game replay.

## Exact next gameplay command (run privately)

    python trace_stage17_obj_uploads.py \
      --rom PSC_STAGE14.gba \
      --sha256 c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609 \
      --state REAL_ACADEMY_MAP18.state \
      --toolkit PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16 \
      --expected-map 18 \
      --route idle:3,right:24,left:24,up:24,down:24 \
      --output PRIVATE_stage17_obj_trace.json

If needed on Linux, set LD_LIBRARY_PATH to the extracted toolkit qa/probe directory before starting Python. Capture the identical route on its correct Stage12 ROM and compare frame/scene/player identity before drawing conclusions about graphics.

## Remaining causal blocker

Use a real mGBA CPU store/DMA watchpoint to record **PC, source bus address, destination OBJ VRAM address, byte length, frame and original actor/animation state**. Correlate those write events with original Genesis sprite table and the captured OBJ hashes. The current Stage17 frame-level source code cannot prove these source addresses: do not use a VRAM tile index or unverified byte search as permanent art mapping.

Live Drive directives confirm accepted production R235 is 33,554,432 bytes, SHA256 4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308; **that ROM was not independently staged and hashed in this run**. The Ryzen remote device was offline. The accepted R235 and R86/R203/R209 archives were not present as verified local binaries. No genuine Academy gameplay or VBA-M/Delta test was run in Stage17. No new playable candidate is claimed.

Keep Stage14 as last previously gameplay-tested experimental cartridge. Preserve production R235, R236, Rykros aurora backgrounds, manga/portraits, ROM size, native graphics, and GitHub main. Do not commit extracted copyrighted ROM/art or private save-state files.
