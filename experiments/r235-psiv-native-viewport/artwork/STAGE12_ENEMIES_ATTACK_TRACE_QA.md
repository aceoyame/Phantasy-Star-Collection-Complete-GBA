# Stage12 — Genesis-source enemy art + verified battle command progression

November 23, 2026. **Experimental branch only**, not accepted or release-ready.

## Exact builds

- Accepted R235 (untouched), SHA-256: `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- Stage11 parent, SHA-256: `820892b5fa2c145a249876baba90144152d7ff1520fb865e43d2103bc2bb80b2`.
- Stage12 test candidate, SHA-256: `5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02`, CRC32 `054392a8`, exactly **33,554,432 bytes**.
- Original US Genesis PSIV source, SHA-256: `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`.
- Patch script: [build_stage12_enemy07_08.py](build_stage12_enemy07_08.py), using R203 reference decompressors, read-only Genesis source, hash guarded parent and output.

## Artwork actually changed

Two additional ordinary enemy variants whose Genesis tilemaps/palettes match the current GBA ROM exactly:
- Enemy `0x07`: native dimensions 64×48; 15 static body tiles, **188 changed ROM bytes**.
- Enemy `0x08`: native dimensions 64×48; 15 static body tiles, **192 changed ROM bytes**.
- Exactly **380 bytes** changed, limited to the two static tile banks. All 12 animation-controlled tiles per enemy remain bit-identical. No ROM header, UI, scenery, sound, camera, controls, story state or battle logic changes.
- Together with earlier Stage7/9 enemy IDs 06, 10, 09 and 0C, this makes **six** experimentally improved enemies. The prior 11 party members' source-interior detail experiment is preserved.
- Accepted full-scene R235 battle backgrounds (including Rykros auroras), 40 experimental manga panels and muted dialogue-portrait border remain unchanged.

## Actual executed emulator checks

1. **24/24** matched battle-snapshot checks (2 enemies × 3 cores × 4 checkpoints). mGBA, VBA-M libretro, and Delta-style VBA-M all booted a real single-enemy battle and displayed changed pixel art in the enemy region only. Enemy07: 256 changed screen pixels at final snapshot; enemy08: 266. No HUD, party or top background changes, battle frame and sampled state matched Stage11.
2. **84/84** longer A/B/idle command input-sequence checkpoints (2 enemies × 3 cores × 14 steps) matched baseline battle frame/phase and retained HUD/background placement. This is meaningfully more than static sprite data verification, but it is **not** a full natural encounter or all enemy death animations.
3. **27/27** title/new-game/opening field/early UI checkpoints (9 samples × 3 cores) exact pixel CRC, map, frame, and player-world-coordinate equality to Stage11. The test was explicitly corrected to compare **Stage11 versus Stage12**, not a ROM against itself.

Total: **135 matched targeted emulator checkpoints** (24 + 84 + 27). All were locally executed with actual GBA code in emulator cores. No claim of broad gameplay beatability or physical GBA hardware testing.

## Attack pose audit (read-only)

Chaz, Hahn and Rika attack source sheets extracted from Genesis and compared to the **existing** Stage12 GBA 36×34 pose records (3, 3, 5 frames, respectively). Existing silhouettes, positioning and basic source composition look closely aligned; we observed internal palette/nearest-source differences, but these are **not proven defects**, given palette projection and sprite transparency. The recorded color-error-over-threshold fractions were approximately 10–21% of opaque pixels across the 11 sampled frames under a naive nearest source mapping. No attack pose bytes were changed. Before doing so, instrument real live attacks with frame-by-frame sprite tile and effect-origin comparisons. Do not replace source-backed poses with a blind resize.

## Overworld and UI remaining

R235 Academy already uses 16×32 live native-pixel OBJ sprites; the exact Genesis actor pose mapping remains unidentified. No overworld asset or menu/control graphics changed in Stage12. UI/GBA dialogue layout and expanded manga framing remain open. Scene-transition native field camera settling is still not fully resolved.

## Acceptance

**Not ready to promote**. Missing full attacks/casts, KO, spell animation, enemy dying, native world actor pose equivalence, original dialogue scenes, full game FIN, physical GBA/Micro, and exhaustive Delta-iOS regression. Do not commit copyrighted ROM/assets to public GitHub.

This stage is a reversible, SHA-pinned source-data experiment.
