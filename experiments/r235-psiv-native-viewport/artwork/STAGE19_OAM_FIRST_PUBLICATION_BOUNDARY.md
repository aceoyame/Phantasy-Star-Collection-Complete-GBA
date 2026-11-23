# Stage 19: earliest OAM publication boundary — live mGBA replay

2026-11-23. This is an **emulator-only controlled diagnostic**, not a new game cartridge or release acceptance.

Exact SHA-256 checked accepted R235: `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308` (33,554,432 bytes). Exact SHA-256 checked experimental Stage14: `c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609` (33,554,432 bytes). Academy state: `3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3`. Actual headless mGBA replay with normal controller route: idle25, right20, idle16, left18, up12, down12, idle55. Same map/player scene guard: map17, world (328,24).

### NEW tested boundary

A new independently restarted mGBA session attempted the *same strictly guarded* OAM RAM-only correction at frames 16569, 16570 and 16571, with a separate accepted-R235 and untouched-Stage14 comparison. **The frame16569 and frame16570 attempts both refused to operate**, because the required sprite tiles were **not yet in OAM**: slot0 attributes were (0x00A0, 0x0000, tile0), instead of the expected shape/tile496. At frame16571 the sprite entries exist, and the diagnostic successfully applied the intended x−16/y−8 offset to slots 0, 2 and 40.

| Frame | Untouched Stage14 RGB mismatch (224×152 common crop) | Stage14 with OAM correction after 16571 |
|---|---:|---:|
| 16569–16570 | 0 | 0 |
| 16571 | 3135 | 3135 |
| 16572 | 681 | **0** |
| 16573–16578 | 0 | 0 |

This confirms the actual OAM publication occurs **after frame16570 and no later than frame16571**, not simply that old sprites have unshifted positions for two frames. It also confirms that the frame16571 3,135-pixel mismatch **cannot be solved by editing these three already-published OAM entries at the previous frame boundary**, because the entries do not yet exist there. A safe source patch must inspect the OAM producer and BG raster publication **inside frame16571**, not just the frame16572 actor-camera hook. Frame16571 remains a distinct unresolved transition/raster issue.

The test used real mGBA emulation and strict OAM shape/tile/map guards, **no ROM edits**, no external assets or original artwork committed. Local private JSON receipt: `stage19_early_oam_private.json`. The diagnostic source is maintained privately alongside the headless toolkit; no Sega assets are embedded here.

### Next source-level gate

Trace the original producer writing slots 0/2/40 into 0x07000000 OAM between the 16570/16571 frame boundaries; correlate with BG raster DMA0 setup at 0x08C7D17C (first hit frame16571). Only then prototype a scene-independent guarded source fix, replay Academy and unrelated entrances, test mGBA and VBA-M/Delta-style cores, confirm 32 MiB, GBA Micro timing and save fidelity. Accepted R235 and production branches remain unchanged.