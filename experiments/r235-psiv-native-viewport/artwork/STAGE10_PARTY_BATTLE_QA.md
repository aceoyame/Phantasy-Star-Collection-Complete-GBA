# Stage 10 — Genesis-source rear-view party sprite detail

**Status: experimental R235 cartridge; production accepted R235 and `main` untouched.**

## Input / code provenance
- Stage 9 parent ROM SHA-256: `53848793971f0fe77c6880560807816652573925c73467f162c4835677ff558c`
- Original US Genesis PSIV SHA-256: `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`
- Original source frame generator: user's R203 archive, `r198/reference_tools/generate_party_battle_frames.py`.
- GBA indexed pose buffer coordinates source: user's R203 archive, `r198/build_r198.py` `PARTY` base pointer table and 36×34 pixel indexed frames.
- The accepted R235 battle backdrops (including complete-sky/aurora scenery), Stage 9 four enemy patches, Stage 6 muted portrait border, 40 test manga panels, and Stage 4 field camera are all preserved **byte-for-byte** outside the six pose buffers.

## Specific source-guided changes
- Chaz: idle **56** opaque interior pixel indices, cast **57**
- Rune: idle **66**, cast **67**
- Kyra: idle **69**, cast **65**
- **380 bytes** modified from Stage9, all in the six exact 36×34 pose records at ROM offsets `0xE4F870`, `0xE4FD38`, `0xE549C0`, `0xE54E88`, `0xE5CAE0`, `0xE5CFA8`.
- Original transparent mask, 1px silhouette boundary, palette bank, positional anchor, battle layout, animation code, map/event state and GBA header are unchanged.
- This source-nearest adaptation is **not** a 1:1 enlargement of the full Genesis 48×48 rear pose, because doing that would clip poses in the actual 36×34 GBA party HUD space. It is a conservative detail pass inside the existing geometry.

**Trial ROM**: SHA256 `d08d6c88b5c6f6dee2ae15925864e5c98d58b113859d063476ac86ea97161d56`, CRC32 `796c7697`, exactly 33,554,432 bytes.

## Actually executed tests
- Genuine battle test with Stage9 vs Stage10, vs enemy `#09`, using mGBA, VBA-M libretro and Delta-style VBA-M, four actual snapshots per core (entry, +20 frames, commands, selected): **12 comparisons passed**.
- All four images in each core differ by exactly **191 rendered pixels** only within x108–203, y99–123 (rear-view party sprites). Outside the party band (y0–84 and y129–159), no pixel changes; screen frame count and sampled state byte match parent at each checkpoint. This is **not** equivalent to a full attack/cast sequence test.
- Independent title/new-game/field/UI replay compared Stage9 and Stage10 at **9 checkpoints × 3 cores = 27 exact matches**, including CRC, map ID, character world coordinates and frames.
- The source-generated idle/cast pixel buffers and exact candidate ROM hash were independently verified from the user-held inputs.
- No user-hardware/GBA Micro test and no end-to-end FIN screen playthrough.

## More party poses and UI
The original source frames for **all 11 party members** were decoded and compared to their matching R235 indexed sprite records. The audit identified **1,182** candidate interior color corrections across 22 idle/casting poses. Only Chaz, Rune and Kyra (six poses, 380 changes) are currently included in the test ROM. The other eight members (16 poses) are **not patched**, pending their real battle scene and action tests. Attack, thrown weapons and movement sprites need separate animation-specific work.

`r191/src/windows.c` already constrains GBA UI rectangles to native 240×160; this stage deliberately does **not** change borders, cursor positions or font sizing. Source authentic UI review remains a separate task.

## Future work gate
1. Exercise cast animations with actual spells in each core and compare sprite timing/effect origins before any release acceptance.
2. Extend the same guarded source-pixel method to Alys/Hahn/Rika/Demi/Wren/Raja/Gryz/Seth, only after authentic party composition tests. Test arms/weapon poses separately.
3. Trace overworld actors and palettes, inspect UI borders/text and battle windows against Genesis while retaining visible art, especially Rykros auroras.
4. Keep the accepted R235 rollback and `main` immutable, 32 MiB ROM, no scaling, GBA Micro compatibility, and full-playthrough/FIN requirement.

The source code on this branch is purely a reproducible patch recipe and contains no proprietary Sega game assets or extracted sprites.