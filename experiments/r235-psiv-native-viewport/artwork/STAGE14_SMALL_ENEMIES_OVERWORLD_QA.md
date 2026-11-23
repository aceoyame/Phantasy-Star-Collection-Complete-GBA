# Stage 14 — small enemy and Xe-A-Thoul sprite art; real overworld pose inventory

**Scope changed:** Per user instruction, stop Dark Force/Profound Darkness and battle-effects work. Prioritize smaller enemy art, suitable normal bosses (three Xe-A-Thoul) and original overworld sprite mapping. **Stage14 is experimental, not production.**

## Authenticated original-Genesis identities
Original US Genesis PSIV (SHA256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`) and original disassembly symbol table `diagnostics76/ps4.constants.asm` from user's private R86 archive:
- Enemy #0A — **Zoran Bult**: source art **48×48 px**.
- Enemy #0B — **Gicefalgue**: source art **48×48 px**.
- Enemy #7B — **Xe-A-Thoul**, **80×80 px**: original Air Castle encounter/event **0x0E** explicitly contains **three** #7B enemies.

Source tilemap and palette entries are byte/word-for-word identical to current R235, so only independently identified non-animated body tiles are replaced. Character placement, palette, target selection, enemy animation destination tiles, party art, special effects and battle scene backgrounds are **unchanged**.

## SHA-pinned ROM and isolated edits
- Parent Stage12 SHA256: `5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02`.
- Stage14 candidate SHA256: **`c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609`**; CRC32 **`a9bf0359`**; exactly **33,554,432 bytes**.
- **583 changed ROM bytes:** 42 (#0A/3 static tiles), 93 (#0B/6 tiles), 448 (#7B/31 tiles). All animated tile records protected and binary-identical; header and all graphics outside the three source-body bank ranges preserved.
- Built twice and compared full SHA256; independent rebuild identical. Source-only [rebuild script](build_stage14_small_enemies_xe_a_thoul.py) is published; user supplies their own original game/earlier experimental ROM.

## Executed emulator tests
**36/36 real-battle A/B comparisons** (3 cases × 3 emulator cores × 4 frames):
- Cases: debug-single #0A, debug-single #0B, original three-enemy **event #0E**.
- Cores: mGBA, VBA-M libretro, Delta-style VBA-M.
- All expected enemy IDs and enemy counts observed in actual game state, not inferred from filenames.
- Frame and battle-state parity with Stage12; every changed on-screen pixel confined to the enemy image region; zero party HUD/background-region changes. For the mGBA 90-frame capture: #0A 52 pixel differences, #0B 95, Xe-A-Thoul three enemies 1,983 pixels across the trio.

**18/18 target-selector paired screenshots** (3 cores × 6 actions), Stage12 versus Stage14:
- In the triple #7B fight, red target marker X centers: **54 → 120 → 186 → 120**, on A/A/RIGHT/RIGHT/LEFT replay. All three enemy targets selectable at their true placement positions. Five-member party HUD remains unchanged.

**27/27 three-core cold start/new game/field/UI comparisons**: exact screenshot CRC, frame, map and player positions against Stage12.

**7/7 genuine Academy save-state field/OAM checks in mGBA**, walking east/west/north/south through a map **18→17** transition:
- All field pixels, map and player coordinates, OAM sprite data, sprite palette and 4bpp OBJ VRAM tile hashes match Stage12.
- Decoded live (not generated) field sprites show **16×32px native** party characters and facing/walking variants. Examples: on map18 loaded slots 0/1 use tiles #448/#480; westward slots 0/1/40 use #480/#448/#488. Map17 transition remains aligned.
- **No new overworld sprite pixels were patched.** A reliable Genesis sprite pose/animation source mapping (including map and actor identity) must be verified before changing any; existing GBA overworld sprites already have native-size geometry.

## User-preserved art direction
- Keep **full-composition accepted R235 battle scenery**, including all Rykros auroras; do not replace with narrow native crop.
- Keep muted dialogue portrait border, existing manga panel trial, earlier source-detail enemies and 11 party pose trials.
- Do not resize or crop giant bosses, rewrite targeting/effects, or alter sprites' size or coordinates.
- No claim of all attack/defeat animation compatibility, a full story playthrough ending at FIN, or physical GBA Micro testing.

Local user-only Stage14 kit includes the actual comparison PNG, seven-checkpoint Academy OAM/directional sprite atlas, SHA-pinned ROM build and full emulator trace JSON. Do not commit original Sega ROMs/assets to public GitHub.

**Next engineering frontier:** use original disassembly and real academy/town save states to identify Genesis field sprite pose table, then implement single-actor source-palette pixel corrections only where proven divergent. Expand ordinary enemy roster similarly, with real battle fixtures, keeping crowded enemy positioning and five-party HUD intact.
