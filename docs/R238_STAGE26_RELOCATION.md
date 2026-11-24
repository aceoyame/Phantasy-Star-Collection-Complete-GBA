# R238 Stage26 relocation — actual Library build, 2026-11-23

**Experimental candidate, not promoted.** SHA256 `9d9ca1b32cfdd844d648fe00caa9549bd702c193d4753d7de9dc5cadd2b365fa`, CRC32 `9f8efe13`, 32 MiB.

Library root contained the exact Stage26 parent SHA256 3154fb729fdb016574d63dff8dea84e01f1b77e2ac346390530d4f90d932a1a0, accepted R237 and the headless emulator toolkit. The original fail-closed merger correctly refused **60,810 conflicting bytes** in the options and map banks. All 158,332 Stage26 modifications were preserved in the rebuilt candidate.

- Exact checkpoint-3 ARM payload (8624 bytes) relocated from 0x0922BA00 to 0x0924E000.
- Map data (57,696 bytes) moved from 0x09238000 to 0x09260000; 6400-byte teleport data to 0x0926E200.
- Verified both new ranges blank in all three SHA-pinned inputs.
- 29 internal pointers, one teleport literal and one split ARM immediate relocated; 231 internal relative branch words verified, 15 game hooks retargeted. Reverting only relocation words yields byte-identical original R238 options code. All 158,332 Stage26 edits remain byte-identical.
- Source-only rebuild: `python3 tools/r238/integrate_r238.py --r237 /private/R237.gba --checkpoint /private/PSC_R238_checkpoint3.zip --stage26 /private/Stage26.gba --out /private/R238_STAGE26.gba --report /private/report.json`. Existing Stage26 wrapper now invokes relocation automatically.

## Fresh real-core regression

Identical Stage26 opening route, parent vs candidate, 3135 frames per run, 69 sampled checkpoints/core:
| Core | Exact checkpoint matches | PCM bit-identical |
|---|---:|---|
| mGBA | 63/69 | No |
| VBA-M | 68/69 | No |
| Delta-style VBA-M | 57/69 | No |

All reached the same tested endpoint; sampled party/map state unchanged. The frame differences and PCM differences must be investigated (new per-field-frame fog hook may shift timing). These results are not full game or menu/map-function acceptance. No actual hardware test. Do not mark accepted or claim timing parity. Traditional R238 remains separate; no renderer work on main.

This supersedes the preliminary Stage26 integration instructions which refused overlapping edits without relocating them.
