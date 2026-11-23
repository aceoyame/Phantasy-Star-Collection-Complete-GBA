# Stage 8 — restore full-scene R235 battle backdrops

**User preference supersedes earlier Stage6/7 native-crop backdrop experiment.**

The user prefers the accepted R235 wide/compositional battle backgrounds, particularly the atmospheric upper sky/aurora-like effects in the purple/green Rykros-type environment. Centered 240x160 Genesis-native crops remove important details. **Keep accepted R235 battle backdrops going forward**. This does not change the priorities for native-size, high-detail character art, enemy art, overworld sprites, effects, portraits, manga and UI, which should still be improved with original source reference, scene-aware framing and real gameplay tests.

## Exact new test-ROM identity

- Input: Stage7 `edf7b67c1c651fed51a12f248aa560c191b3ba0cf70f003fb71d7136b0f645be`
- Accepted R235 art reference: `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`
- **Stage8 candidate: `7d5afd0b0472542a700aa6af8e8c46dc7a86115cf18ad3567d4bb5f753b08fff`**, CRC32 `d856d83c`
- 33,554,432 bytes, 137,417 bytes different from Stage7, all confined to 13 backdrop pointers and now-unused background image data
- Restored table entries for indices **0, 2, 4, 5, 16, 17, 20–26**, including 21
- Free space reclaimed: **140,786 bytes** from `0x0924CE1A–0x0926F40B`
- Stage7 enemy sprite patches 0x06/0x10, 40 manga panels, muted Genesis-style portrait borders, field camera and all other ROM data are byte-identical to Stage7

## Actual tests already performed

1. All 13 battle backgrounds were decoded through the inherited R199 battle row algorithm from accepted R235 and new Stage8: exact **240x160 indexed-pixel parity** for each, including index 21.
2. Real native debug encounter with battle background 0 (Motavia): **three checkpoints per core** (command, commands-open, selected) in mGBA, VBA-M and Delta-style VBA-M. Stage8 frames match accepted R235 **0 differing RGB pixels** across all nine checkpoints; gameplay frame and battle state agree. Stage7 differs from Stage8 by ~18,000 background pixels each.
3. Nine title/opening/field/UI checkpoints in each emulator core (27 total) have identical screenshot CRCs, map IDs, player positions, frame counters against Stage7.
4. Full enemy graphics bank and 39 portrait records are identical to Stage7. The protected Stage7 enemy static tiles remain present.

**Remaining limitation:** Index 21's full *live Rykros encounter* and animated aurora behavior have **not** been played through here; the exact indexed backdrop bytes are restored, but do not conflate that with an animation proof. Full story through FIN, GBA hardware and Micro compatibility are untested.

## Next work

Keep scenic backdrops on accepted R235 for new changes. Prioritize paired source/screen comparisons for (a) party battle poses & attack/cast anims, (b) other enemy idle/hit/death frames, (c) field actor sprite tiles and NPC gestures, (d) battle/UI cursors, glyphs, window geometry, and (e) composition-aware remaining manga scenes. Preserve R235 story/save state and already-fixed bugs; no wholesale rescale. Stage4 camera scene-transition settling remains a blocker for release.

Rebuild source: [restore_r235_backdrops.py](restore_r235_backdrops.py). Source-only repository; **do not upload ROM binaries or original Sega images**. QA images/data available in the private Stage8 test kit.
