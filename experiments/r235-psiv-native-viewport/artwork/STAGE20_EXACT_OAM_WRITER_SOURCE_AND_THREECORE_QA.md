# Stage 20 — instruction-exact Academy OAM source trace and three-core field regression

**Date:** 2026-11-23. **Status: new confirmed first-writer/source-address evidence, NO new playable build or permanent graphics patch.** Experiment only on branch experiment/r235-psiv-native-viewport; accepted R235/R236/main unchanged.

## Control and SHA-verified real inputs

Read current PSC Engineering Worker installed skill and live Google Drive PSC_WORKER_DIRECTIVES, document ID 1-RLEVE-uPiguM0CCwJDqlSKMC_AHGqQ9Z-nHgsyuE-I, revision AHj4eMSAUZAcFOaQJ1Kh5Rr2LutyJSuDgn7zoQNkS7ax3_UTNfo6nTUKtIPlWi60R6KRkL3OI-IYmLIfrVvLZoMpjlz1T9bTeh1O3SdY33I. Live policy: R235 QA-accepted, R236 is separate production lane, this experiment is not a release.

Real user-provided binaries materialized and independently SHA-verified in execution runtime:

| Asset | Bytes | SHA-256 |
|---|---:|---|
| PSC_R235_FIXB_NEWGAME_RELEASE.gba | 33,554,432 | 4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308 |
| PSC_R235_STAGE12_SIX_ENEMIES_PARTY11_TEST.gba | 33,554,432 | 5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02 |
| PSC_R235_STAGE14_SMALL_ENEMIES_XE_A_THOUL_TEST.gba | 33,554,432 | c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609 |
| Authentic after-principal Academy map18 mGBA state | 397,312 | 3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3 |
| Original US Genesis PSIV ROM | 3,145,728 | 511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a |

The original Genesis asset was retrieved from project Drive and verified by SHA plus file's Sega Genesis cartridge header; connected file metadata incorrectly labeled it audio/mpeg, which does NOT reflect actual file contents. Preserve this original input private; no extracted artwork was published.

Headless native mGBA probe SHA256 d40365551067436ee46714e77a4fec80fb1bbbeaec70ba99d6c8f0de80eb7768, libmgba.so SHA256 638f351fdc5b55275c87164bbc5311b650ba488a5be0817a3d8b68fa81b9ad39. VBA-M libretro SHA256 95ae89f99767cb3ce6748ccd437600a11d0cbcbe52b9c5ec1519a1d3a99c1399; Delta-style libretro SHA256 3a340793b19315e60744caa9e77eeb036146e981e209bff15a9be45a318df517.

## New genuine emulator step tracing

Replay from the authentic map18 state with normal inputs idle25, RIGHT20, idle16, LEFT18, UP12, DOWN12, idle55. Map changes 18→17 and the prior Stage19 visible OAM boundary was reproduced. Starting from frameCounter 16570, single-step real mGBA CPU instructions until 16573 and inspect OAM slots 0/2/40 after each instruction:

- Accepted R235: **219,917 exact ARM CPU steps; 23 actual OAM writes affecting tracked slots**.
- Experimental Stage14: **219,933 exact ARM CPU steps; 25 actual tracked OAM writes**.
- FrameCounter 16570 encompasses the CPU OAM stores that appear in framebuffer frame16571; a frameCounter printout is not itself the screen-flip time.
- First OAM publication into the three slots happens in the original game-generated IWRAM routines. The subsequent ROM routine reorders/copies the staged attributes into live OAM.
- The **first offset-specific differing Y/X values** occur in the subsequent update while frameCounter=16572. Before that, the original and Stage14 first new-room sprite attributes are unshifted/identical. After the actor camera hook runs, Stage14 positions become intended x−16,y−8, with geometry and graphics bytes unchanged.

**Important debug-PC correction:** The mGBA host bridge reports the ARM pipeline-prefetched register PC. In these traced ARM stores, the real executed instruction address is **PC−4**; previous unadjusted breakpoint addresses would be one instruction too late. Explicit opcode bytes were read at PC−4 and disassembled with ARMv4T LLVM.

Source-backed write examples:

| Actual executed instruction | Action | Observed Stage14 effect |
|---|---|---|
| IWRAM 0x03002AC8 (STRH r7,[r5,r1]) | OAM slot2 attr0 | Y component shifts from accepted 0x66 to candidate 0x5e |
| IWRAM 0x03002AF8 (STRH r1,[r5,r7]) | OAM slot2 attr1 | X component 0x28 → 0x18 |
| IWRAM 0x03002C5C (STRH r1,[r2]) | OAM slot0 attr0 | Y 0x56 → 0x4e |
| IWRAM 0x03002CC0 (STRH r0,[r2,#2]) | OAM slot0 attr1 | X 0x98 → 0x88 |
| IWRAM 0x03002DC8 and 0x03002DE8 | OAM slot40 position | Same 8-pixel Y and 16-pixel X shift |
| ROM 0x08C3F764 (STRNE) and 0x08C3F788 (STRHNE) | Secondary cached OAM publication | Copies current staged attributes; actual source opcodes unchanged |

The **same native OAM writer machine code exists in accepted R235, Stage12, and Stage14**. We found FIVE 256-byte IWRAM windows with **unique exact original-ROM counterparts** in each file; examples runtime 0x03002AC0 → cartridge bus 0x09EEEB70; runtime 0x03002CC0 → cartridge bus 0x09EEED70. SHA256 of each matching 256-byte window was independently equal in all three ROMs.

This identifies the exact *patchable ROM backing* of the source and rules out a mutable sprite VRAM slot as a safe modification site. **It does not justify modifying the writer yet**: an unconditional OAM offset here would double-shift already-updated sprites and harm unrelated maps, transitions, enemies or save-state fidelity.

Private executed trace receipt SHA256:
- Accepted original frame-by-frame instruction events: f84ed2496ec6fe920571d32f9d4862dae56485cbc8b100a638d1d4a9f937de37.
- Stage14 event trace: 0812bbcf6b81a9220852d22d2e36a237741dff73cf9d51bf607407cd73d14f50.
- Both original/candidate live ARM register snapshots: 4bce1f74b4714a84946af6afa430f9492b050a5f783cf72f2f4ebcadd4d92f66.

## Fresh real three-emulator regression — completed

Stage12 versus Stage14 on actual PSIV cold launch → game startup → early field/map94 with 15 distinct checked user input/checkpoint states (including LEFT/RIGHT/UP/DOWN and menu) per core. Recorded **the same emulator core, frame index, map ID, world player coordinates and RGB framebuffer CRC** on both ROMs; never compare raw CRCs across different cores.

| Core | Passed / tested |
|---|---:|
| native mGBA | **15/15** |
| VBA-M libretro | **15/15** |
| Delta-style VBA-M libretro | **15/15** |
| **Total** | **45/45** |

Real execution ran to frame **4189**, map **94**. Private replay evidence JSON SHA256 **3061ea664c5bc4a955c4a7b1cd80ed619975ec327398f8f1ad0794a1554ff8c1**.

New public **source-only** repro tools: [trace_stage20_oam_writer.py](trace_stage20_oam_writer.py) and [verify_stage20_threecore_field.py](verify_stage20_threecore_field.py). They require separately supplied exact ROMs, state and emulator core toolkits. No commercial ROMs, artwork or private game states were committed.

## Next blocker / QA acceptance

**Root remaining:** determine whether the post-transition BG raster offset can be deferred, or the initial OAM camera shift can be synchronized, **without waiting for the later actor hook or double-applying it**. Locate a true map-entry/actor-ready flag or frame-production ownership check, rather than hard-coding map17 or blindly patching the OAM store. Then produce a guarded optional candidate ROM and replay at least Academy, unrelated room transitions, dialogue, menus, battle positions, original Genesis artwork, both independent cores, save/load, and default GBA/Micro timing.

The 16571 first reveal (3,135 overlapping RGB mismatches) and 16572 sprite lag (681 overlapping mismatches) remain *unfixed in cartridge code*. The previous RAM-only temporary oracle is not a production feature. No new ROM, two-core Academy acceptance, complete story FIN, original Genesis pose mapping, or Micro hardware test is claimed. Full R86/R203/R209 exact source ZIPs were not staged as verified archives this run; R234 source tools archive was available. Production R235/R236 and GitHub main remain unchanged.