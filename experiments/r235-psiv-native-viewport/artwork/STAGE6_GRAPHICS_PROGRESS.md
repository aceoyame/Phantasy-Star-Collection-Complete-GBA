# Stage 6 — PSIV Genesis-native graphics expansion (2026-11-22 ET)

**EXPERIMENTAL ONLY.** No copyrighted Sega ROMs, images or asset data in this Git branch. User-held R235, Genesis US ROM, and R175/R203/R234 archives are required.

## Provenance
- Accepted 32 MiB R235 SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`
- 3 MiB original US Genesis PSIV SHA256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`
- Prior Stage 5 10 manga + 39 portraits + field crop SHA256 `c95c2830de0cfe6bb271edfbfe6b8ea26f32a960dffd3e9818c390713529c954`

## New code/data experiments
1. **Portrait border audit:** Exact extraction of Genesis 48×48 original portraits proves Stage 5 already has its 1px gray outline and 1px white inner border; all 39 × 40×40 records match the intended source-border and native-face indices. User finds the small bright white frame distracting. A clearly labelled *optional GBA-muted adaptation* changes just the 1px white inner line (color index 15, RGB555 `0x7fff`) to existing silver palette color index 2 (`0x5ad6`). 5,772 portrait border pixels changed, across 4,368 ROM bytes, no palette table or face change. **This is not pixel-exact Genesis border color**; retain the exact-source alternative.
2. **Manga 10–39:** Added 30 more Genesis-native indexed precompiled panels, now 40 total. Same common source 240×160 crop (x48,y0) and R234-style compressed spans. All **40/40 executed via the genuine R235 ARM manga decoder in mGBA** and framebuffer bytes exactly match intended crops. Additional 30 panels retain ~92.82% source colored pixels in aggregate; the worst individual only 72.73%. **Some compositions visibly clip**, so this is not accepted for story presentation until per-scene framing and animation are solved.
3. **Battle backdrops:** Indexed IDs **0, 2, 4, 5, 16, 17, 20–26** (13 in all) were decoded from actual Genesis US art at 1:1 pixels, cropped x40,y32,w240,h160 and palette-index mapped to the unchanged R235 battle palette. Added a pure lossless row backreference encoder for the inherited R199/R235 battle format. Each 160×240 decoded indexed image and actual RGB555 colors match source crop. 13 records occupy 6.4–14 KB apiece (versus 39 KB literal). ROM table pointers updated in private test ROM only. The final allocated ROM address is `0x0926F40C`, below the certified `0x092701F0` limit; ~3.5 KiB free remains. **Other special backdrops have palette issues and are NOT patched.**

## Actual emulator testing
- mGBA and VBA-M: 9-checkpoint cold launch/field parity against Stage5; no changed screenshots, player coordinates or maps.
- Delta-style VBA-M: separate nine-checkpoint baseline-versus-final cold launch/field test PASS.
- Real debug test-case #2 (Motavia background index 0) command / commands open / selected screens were played in **mGBA, VBA-M and Delta-style VBA-M**. All show native 1:1 background with visible sprites and HUD; battle state and command phase matched.
- Compare **literal native** vs **losslessly compressed native** battle #0: exact framebuffer parity at checkpoints 2 and 3 in all three cores. Initial checkpoint differs by 1,032 pixels confined to animated enemy sprite x95–144,y45–85, while battle background and HUD match. Investigate timing; do not call all frames identical.
- **No real full-story dialogue/manga transition playback, full-game FIN run, hardware/GBA Micro test, or release acceptance.** Camera scene-transition settle issue remains open.

## Reproducible experimental cartridge hashes (each exactly 33,554,432 bytes)

| Candidate | SHA256 |
|--|--|
| 40 manga, source-faithful portrait border, inherited Stage4 camera | `5d2a21888887937ff8782532df6bf18c80a064e04a53a9c5897e8678f6167b69` |
| 40 manga + muted portrait highlight, inherited Stage4 camera | `7dadb9992eeaf0bcc0059dda7268650419a7bc6762614d1b9743600bff70c918` |
| 40 manga + 13 source-native battle backgrounds + muted portrait highlight | `e8d5418560cdd43fac46a6e5214b7adc691c17b8f4e3bc1eddb58741a77f57b0` |
| 40 manga + 13 source-native battle backgrounds + exact Genesis portrait border | `4dade6c06e0d2cbc3d17cbbc20a5bfb475c4a71fa2132512132ad8a65dca9617` |

## Next engineering tasks
1. Do not blindly expand remaining manga panels: preserve whole comic composition via scene-aware camera/crop, support all 166 main indices + 3 extensions and test through story.
2. Real live portrait playback in dialogue, character expression/palette, compare 1px gray/white Genesis border with optional GBA gray/silver variant on GBA hardware.
3. Verify all 13 new battle backdrops against enemies, effects, layer priority, battle HUD and movement. Check performance and first-checkpoint enemy animation divergence. Audit missing source palette colors for special backdrops.
4. To expand all battle/manga art without exceeding 32 MiB, reclaim old unused compressed panel/backdrop streams safely after all references migrate, or introduce compact shared assets. Preserve map/event data and the R231 rollback.
5. Continue original-art treatment to remaining sprites, enemies, special effects, vehicles, cinematics and UI; no scaling, no faster-EWRAM hack, no unsupported quality shortcuts.

Full reproducible build scripts, hashes, and local three-core evidence were produced as a separate user artifact; the exact Stage6 scripts do not embed any original Sega assets. This record is for the scheduled worker to resume from a **real, source-backed frontier**, not to claim overall graphics completion.