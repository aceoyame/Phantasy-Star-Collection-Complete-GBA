# Stage 13 — oversized bosses, Profound Darkness transitions and five-enemy geometry

Date: November 23, 2026 (US Eastern). **QA and source audit only; Stage12 remains the latest experimental playable ROM.** No boss, formation, or effect patch has been shipped in this stage. Accepted R235 and GitHub main remain unchanged.

## Authenticated inputs
- Accepted PSC R235, exact 32 MiB, SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- Stage12 experimental 32 MiB, SHA-256 `5ff6d48606576e19ee46d14d7d8892b9835d4161afebde74f781caf76d38ff02`.
- Original US Genesis PSIV SHA-256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a` from the user's private R175 archive.
- Historical R192 effects code and baseline ROM from the user's R203 archive.
- No original Sega binary assets have been committed to GitHub.

## Why oversized bosses MUST NOT use a universal 240-pixel crop
Directly assembled original Genesis static boss art (Nemesis/Enigma asset decoder), counted **opaque source pixels** excluded by 240px horizontal crop. This is a *source-art geometry study*, not a source-to-GBA timing match.

| Boss form | Native art dimensions | Center crop losses | Best possible fixed crop losses |
|--|--:|--:|--:|
| Dark Force 1 (82) | 320×120 | 7.45% | 7.28% |
| Dark Force 2 (83) | 320×120 | 12.64% | 12.64% |
| Dark Force 3 (84) | 288×120 | 1.03% | 0.00% (x46) |
| Profound Darkness I (85) | 320×120 | 11.75% | 11.73% |

**Profound Darkness II / III:** both source asset descriptors are 320×120, but generic standalone Genesis tile-map decoding encounters sentinel index 0x7FF; the dynamic, layered native battle compositor must be used to interpret them. Do not treat missing generic decode output as proof of broken graphics. Neither form is suitable for blind global cropping.

Production-friendly policy: preserve full silhouette, authored layers and effects, background scenery, hit-target placement and HUD. Use scene-specific projection or rearrangement only when actual source/gameplay parity proves it. Retain accepted R235 full-composition battle backdrops, especially auroras on Rykros.

## Actually executed live R235 battles
- The native event driver loaded **Dark Force 3 (event 12), Profound Darkness I (1A), four-enemy event 0D** in accepted R235 and Stage12. Their enemy IDs/counts matched.
- Three emulator cores **mGBA, VBA-M libretro and Delta-style VBA-M**: 4 matched-input checkpoints × 3 battles × 3 cores = **36 paired comparisons**. At every checkpoint the top 86 rows, containing the sampled boss/formation and background, are pixel-identical between parent and Stage12. Battle frame, enemy ID, enemy count and sampled battle state match; party/HUD pixels may differ due to earlier artwork changes.
- Live four-enemy event 0D: count=4, enemy IDs `6D 6D 6D 6D`, 62-byte actors, slot layout bytes at EWRAM `0x02028D06` are **8,16,24,32**, corresponding to actual X centers **48,96,144,192**. Screenshots confirm four distinct visible enemies.
- With *test-only HP cap and party-state assistance*, the Stage12 Profound Darkness event transformed through all three forms: 85→86 at tick1200, 86→87 at tick2743, and form87 captured through tick2924. Accepted R235 transformed at **identical ticks** in the same controlled run. At 3 matched frame offsets in each of the 3 forms (9 comparisons), **top 88 rows remain pixel-identical**, including those visibly animated overlays; total image differences are solely in party band x36..203, y99..123 (271 pixels at each sample).
- Those controlled transformations **do not establish** original Genesis fidelity, unassisted battle beatability, graphics correctness throughout the entire animation, or reaching FIN. The third form includes complex moving color-band overlays and must be compared frame-matched to Genesis before any graphic rewrite.

## Verified four-slot effect-target limitation and 5-enemy risk
R235/Stage12 Thumb helper `0x087F9A9C`, with branch at `0x087F9AAC`, contains this exact behavior:
- `cmp r0,#3`; for a requested enemy index beyond 3, `movs r0,#120` and `bx lr` (center-screen fallback).
- For indices 0–3, reads layout bytes from `0x02028D06+index` and derives X as 6×(byte&0x7F).
- R192 historical battle-effect source `r192/src/effects.c` contains `int anchors[8]` and `for (unsigned i=0; i < ENEMY_COUNT && i < 4; i++)`. The same three effect-entry hooks at `0x087F8574`, `0x087F868C`, and `0x087F8708` remain byte-identical in R235 and Stage12. Within the original 2,136-byte R192 effect-body interval only **14 bytes** differ in R235; the historical source therefore remains highly relevant, but the entire active R235 runtime call chain must still be profiled.

**Interpretation:** slot #5 may be omitted from authored spell/effect anchor roles or wrongly targeted at x120. This is a real **risk** that now has a reproducible code-level gate, not confirmed evidence of visual crowding or a fifth target being untargetable in ordinary gameplay. **Do not patch slot #5 with an arbitrary X offset.**

Historical R200 encounter suite covers 153 fixtures (71×1, 68×2, 13×3, 1×4), none with five; this is incomplete coverage, **not proof** that the game cannot generate five enemies.

## Required fix/acceptance gate
1. Locate an authentic original-Genesis **five-enemy encounter** and record IDs, source input, world/map, backdrop, formation index, target order, object widths and reference frames.
2. Replay the same encounter in accepted R235/Stage12 in two cores, reading live `0x02024000`, `0x02024044`, `0x02028D06..`, Mode4 pixels and associated per-slot target highlights.
3. Verify all five are visible, not cropped or silently occluding each other; enemy animations and command/target highlights match their IDs; area and single-target magic/effects reach target #5. Preserve the five-party-character HUD.
4. Trace and update **formation writer + rendering + target selection + technique/skill/combo anchors together**, replacing the hard-coded four-slot assumptions only with grounded layouts. Test one through five enemies plus Dark Force/Profound Darkness.
5. Check battle transitions, victory/defeat, performance, GBA Micro hardware and eventual unassisted full-game ending before production promotion.

The present evidence explicitly **blocks a mass crop/resize of oversized bosses and a speculative five-slot patch**. The next productive step is capturing a source-authentic five-enemy encounter, not editing bosses or formation coordinates by guesswork.

See the local Stage13 QA package for exact source-art crop images, generated live emulator screenshots, source guards and executed trace JSON.
