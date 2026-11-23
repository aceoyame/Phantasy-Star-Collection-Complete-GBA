# Stage 13 — oversized bosses, Profound Darkness transitions and authentic four-enemy formations

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

## Verified Genesis-style four-enemy formation and five-party distinction

**Correction after source and gameplay cross-check:** Phantasy Star IV permits **five party members** and appears to permit **up to four enemies** in normal battles. The original game references and R235's actual formation renderer agree. This replaces the earlier assumption that five enemies were a normal source-original encounter.

The four-enemy live event 0D has placements [8,16,24,32] and enemy centers [48,96,144,192] on the 240px GBA screen. All **five party characters and their HUD rows** are visible in the same captured scene.

The R235 Thumb helper at `0x087F9A9C` has `cmp r0,#3` then a return of x120 for an enemy slot index 4+. Its historical R192 effect code has four enemy anchors (`anchors[8]` reserves 3 general roles + target + 4 enemy roles). The helper, three entry hooks, and this legacy cap are **consistent with the original four-enemy design**, **not a confirmed software bug**. A hypothetical ROM-hack five-enemy formation would be out of source scope and must not drive changes to the faithful port.

The historical R200 153-encounter sample contains 71 one-enemy, 68 two-enemy, 13 three-enemy, and 1 four-enemy case; no five-enemy sample. Multiple independent game descriptions corroborate that four enemy combatants and five allies are the normal caps. This sample alone is not an exhaustive formation decoder.

**Acceptance priority is real 1–4 enemy encounter parity while showing all five party members**, including command selection, area effects, target arrows and mixed-width enemies. Do not invent a fifth enemy slot or resize all enemies to make space.

## Required next acceptance tests

1. Enumerate source-original 1–4 enemy formations, including the widest mixed-size combinations, and replay target order/reticles on accepted R235 and the experimental branch.
2. Verify the five party members' rear-view poses and complete five-column HUD are not clipped when four enemy sprites and long technique/effect animations are displayed.
3. Use original Genesis reference scenes for Dark Force 1–3 and all three Profound Darkness forms, and preserve their authored art and layered animation (not a universal static crop).
4. Before any boss changes, confirm full-screen Mode4 ROI, background, attacker/target alignment, KO and defeat effects, timing and input/replay state on mGBA, VBA-M and Delta-style cores.
5. Preserve full background compositions (Rykros auroras), 32 MiB and GBA Micro hardware requirements; no main-branch promotion without full story to FIN.

The present evidence explicitly **blocks a mass crop/resize of oversized bosses and any speculative fifth-enemy extension**. The next productive step is capturing a source-authentic five-enemy encounter, not editing bosses or formation coordinates by guesswork.

See the local Stage13 QA package for exact source-art crop images, generated live emulator screenshots, source guards and executed trace JSON.
