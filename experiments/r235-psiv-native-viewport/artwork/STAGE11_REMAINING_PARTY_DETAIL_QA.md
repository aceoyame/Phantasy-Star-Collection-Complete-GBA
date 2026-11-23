# Stage 11 — eight remaining party members, controlled source-detail experiment

**Status: experimental source patch + candidate ROM. Not a release candidate.** No Sega ROM, extracted sprite image, or proprietary binary is committed to this public branch.

## Provenance and exact candidate
- Accepted R235 parent remains immutable: SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- Stage10 parent SHA-256: `d08d6c88b5c6f6dee2ae15925864e5c98d58b113859d063476ac86ea97161d56`.
- Original US Genesis PSIV source SHA-256: `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`.
- Stage11 candidate SHA-256: **`820892b5fa2c145a249876baba90144152d7ff1520fb865e43d2103bc2bb80b2`**; CRC32 `4dd9ba86`, exact 33,554,432 bytes.
- Script: [build_stage11_remaining_party_detail.py](build_stage11_remaining_party_detail.py). Operator supplies their own SHA-verified Stage10 GBA ROM and `r198/reference_tools/generate_party_battle_frames.py` output from the user-owned R203 source/Genesis archive.
- Additional **802 changed bytes**, solely within 16 static indexed 36×34 battle-pose buffers; no GBA header, code, palettes, transparency masks, sprite anchors or battle HUD changes.

## New party artwork
| Party member | Idle pixel indices changed | Casting-source indices changed |
|---|---:|---:|
| Alys | 36 | 38 |
| Hahn | 43 | 54 |
| Gryz | 35 | 50 |
| Rika | 34 | 37 |
| Demi | 46 | 60 |
| Wren | 79 | 87 |
| Raja | 46 | 52 |
| Seth | 53 | 52 |

Stage10 already added Chaz, Rune and Kyra. All 11 party members now have an **experimental source-guided internal color pass** for the first two pose records. However, the 48×48 original Genesis compositions are still being presented in the inherited 36×34 GBA layout; **this is not a full native-size sprite conversion**.

## Executed QA
1. ROM SHA-pinned script rebuilt the exact candidate, with 802 changed bytes, all within the 16 expected 36×34 buffers. Transparent pixels and outer silhouette boundaries are preserved.
2. **27/27 cold-boot/new-game/field/UI checkpoints match Stage10** in mGBA, VBA-M and Delta-style VBA-M: same screenshot CRCs, frame counts, maps and player world position. The tested route doesn't exercise new characters.
3. **16/16 actual mGBA battle sprite decoder/drawer tests**: each of eight characters' idle and casting source frames was transplanted *for test only* into the displayed Chaz slot in a temporary debug battle ROM. This exercised the native sprite renderer and showed visible changes only inside the party sprite area for each pose; zero enemy/backdrop/HUD changes. These are not authentic alternative-party encounters and the 'cast source' was displayed in a static slot, not through a magic cast transition.
4. **12/12 VBA-M and Delta-style VBA-M in-engine diagnostic tests** for Alys, Wren and Seth, both source pose records, using equivalent temporary Chaz-slot art transplants: pixel changes solely inside the party sprite region, game-state parity versus before art.

## Intentionally unchanged
- The complete-composition **accepted R235 scenic battle backgrounds**, including Rykros auroras (per user's preference).
- The muted portrait border variant, existing 40 experimental manga panels, 4 source-enhanced enemies, prior Stage4 field camera, and existing UI.
- Original game AI, progression, palette indices, effects timing and hardware timing.

## Acceptance blockers
- Real party formation for each character and full **cast, attack, movement, hit, KO** animation sequences have not been independently replayed end to end; changing source pose bytes safely does not prove spell use.
- Large full-frame 48×48 native-size character rendering plus coherent GBA HUD reflow remains a bigger, unimplemented project.
- Overworld NPC art, vehicle art, menu glyph comparison and remaining enemies/manga remain unfinished.
- No physical GBA Micro validation, long-run timing benchmark, full-story FIN playthrough, or formal release approval. **Do not promote Stage11** to main or accepted R235.

The test kit may contain images and ROMs privately for the user's own testing, but proprietary assets must remain outside public GitHub.