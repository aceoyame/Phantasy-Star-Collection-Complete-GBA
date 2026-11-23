# Stage 17 addendum — actual 158-frame Academy mGBA replay and map-transition camera gate

Date: 2026-11-23. **This supersedes Stage17's earlier "no genuine Academy replay / accepted ROM not staged" limitation.** Original ROMs, emulator state and screenshots remain PRIVATE and are not committed.

## Real inputs independently staged from the owner's ChatGPT Library

| Input | Exact verified SHA-256 | Size |
|---|---|---:|
| Accepted production R235 | 4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308 | 33,554,432 |
| Earlier experimental Stage12 | 5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02 | 33,554,432 |
| Latest patched experiment Stage14 | c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609 | 33,554,432 |
| Authentic Academy settled mGBA state extracted from user's R235 overnight checkpoint kit | 3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3 | 397,312 |

Emulator: extracted native mGBA headless bridge from the user's Library PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16.zip (probe.so/libmgba.so/mgba2.py). The state loads at Academy map 18. Inputs are ordinary per-frame controller inputs; **no RAM patches, ROM writes, skipped game frames, changed emulator timings or faster-EWRAM hacks**.

Live Drive PSC_WORKER_DIRECTIVES (document id 1-RLEVE-uPiguM0CCwJDqlSKMC_AHGqQ9Z-nHgsyuE-I) confirmed R235 accepted, and R236 production work remains separate. Source-tools R234 archive was staged. Full R86/R203/R209 source archives and equivalent VBA-M save-state replay were not independently validated this run.

## Test 1: exact live OBJ mutation recorder

Using the source-only [trace_stage17_obj_uploads.py](trace_stage17_obj_uploads.py), execute identical Academy routes on Stage12, Stage14 and accepted R235:

- Route: idle 25, RIGHT 20, idle 16, LEFT 18, UP 12, DOWN 12, idle 24 = **127 emulated frames per cartridge**; exact PC frame interval **16446–16572**.
- Each cartridge has **112 recorded OBJ/OAM/palette mutation frames**.
- **Stage12 versus Stage14: 112/112 exactly equal** on all 14 recorded source fields, including screenshot CRC32, map, player, OAM, OBJ VRAM, object palette and per-sprite byte hashes.
- 37 frames show OBJ VRAM changes; **296 changed 32-byte OBJ blocks** across 24 tile slots (448–455 and 480–495). These are dynamic VRAM *destinations*, not stable character identities or original ROM source addresses.
- Accepted R235 versus Stage14: same map, player, frame count, 32 KiB OBJ memory and OBJ palette on all 112 mutations; OAM and screen CRC differ on 110/112 as expected from the intentional camera crop. Of **333 matched visible sprite observations**, **330 move exactly x=-16,y=-8** relative to R235 with **unchanged raw sprite pixels and palette**; **3 transitional observations** at the first fully rendered new-room frame are initially unshifted.

Private local evidence JSON SHA-256: accepted trace 78d360985d1d8a088715c40d4208896d819d3ba25b269664cd9b82eaa37c14dd; Stage12 trace 23be76357d8015aa32f1af467778866ecfac75ec8f508012fdd88b8504fb3f38; Stage14 trace 72001f1c60682d1e17724781fcce8bd101dd7689daaae3cd097158dc541df5b1.

## Test 2: newly implemented complete-screen three-ROM gate

New [verify_stage17_academy_threeway.py](verify_stage17_academy_threeway.py) validates user-provided three ROM hashes and state, then independently replays **158 frames per cartridge** (same sequence plus 55 final idle frames) using the actual mGBA core. Unlike the mutation-only tracer, this gate computes the **whole 240x160 framebuffer CRC, map, world player coordinates, all-OBJ SHA and palette SHA on EVERY frame**.

**Stage12 versus Stage14: 158/158 identical** on all six evaluated fields. **Accepted R235 versus Stage14: zero divergences** across frame indices, map IDs, player world coordinates, OBJ graphics memory, and OBJ palette; screen CRC differs for 142 of 158 frames intentionally due to the 16x8 crop. Map changes **18 to 17 at frame 16555**, without different game state. The 158-frame gate output JSON SHA256 (local, PRIVATE): 10372194c994f428e2e6f7931557f95e9cdb7863fc38e575c83e71e8c6021eab.

Original/Stage14 frame classification using exact 1:1 pixels, no image resampling:

| Exact mGBA frame range | Frames | Observation |
|---|---:|---|
| 16446–16471 | 26 | Initial state-load BG/OBJ camera warmup; not yet shifted-overlap exact |
| 16472–16554 | 83 | **100% pixel-exact**: Stage14 visible overlapping 224x152 region equals accepted R235 shifted by (16,8) |
| 16555–16570 | 16 | Two cartridges exactly equal full-screen during map17 transition/blank-scene interval |
| 16571–16572 | 2 | Short new-room BG/OBJ settling mismatch; 1:1 shifted overlap roughly **90.8%** then **98.0%** matching pixels in RGB comparison |
| 16573–16603 | 31 | Restored **100% pixel-exact** 16x8 overlap through remainder of new-room idle test |

**New specific acceptance blocker:** the **first 2 frames after the new map becomes rendered** temporarily fail BG/OBJ crop parity. We inspected actual private emulator screenshots at frame 16571; BG is shifted while the field sprites have not yet moved to the corresponding offset. At frame 16573 the BG and OBJ crop match completely. This is not a proven map collision, save, timing or sprite-art defect; it is an observed two-frame presentation transient, consistent with the earlier Stage4 transition warning. Investigate field actor publication/scroll-buffer order after transition, and only patch a source-proven frame producer.

## Implementation, unchanged ROMs, next gate

This run added one **source-only read-only OBJ frame tracer**, unit tests (three passed), and a separate SHA-gated **full-frame three-ROM replay verifier**. It produced **no new cartridge** and did not change R235, Stage12, Stage14, production R236, GitHub main or commercial assets. The two live paths above used real mGBA execution, but **VBA-M/Delta on these exact map-transition input frames, active dialogue, battle menus, original Genesis pose mapping, save/load, full FIN playthrough and physical Micro timing remain unverified**.

Next executable action: instrument the **frame 16570–16573 BG raster-scroll producer and actor OBJ/OAM update** to find why BG and OBJ update on separate frames after map17 loads, and separately trace CPU/DMA source PC, source ROM address and byte count for mutable VRAM slots. Do not patch source based only on a tile-index hash or assume a global rescale. Preserve accepted scenic Rykros/aurora battle backdrops and original screen compositions.
