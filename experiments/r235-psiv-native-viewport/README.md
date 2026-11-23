# R235 PSIV native-viewport test branch

**Status: experimental source module, not yet a live R235 runtime patch.**
This branch is separate from `main`. It contains no game ROM, commercial game images, or extracted assets.

## Why cropping is more appropriate than scaling

The R235 Academy visual audit compared 9,900 registered static wall pixels. Tile silhouettes matched one-to-one, with 11 original colors each mapping to exactly one GBA color. Therefore, at least this field scene already preserves Genesis tile geometry; a global rescale is unnecessary and risks losing details.

## What is implemented

- Pixel-perfect camera initial placement from 320x224 to 240x160
- Smooth dead-zone tracking and world-edge clamping
- Sprite and object clipping with no resampling
- GBA-safe dialogue portrait, battle command and status UI rectangles
- Host-run C assertions and a read-only test against the actual R235 visual QA screenshot archive

Run host tests in this folder:

```sh
cc -std=c11 -Wall -Wextra -Werror psiv_viewport.c test_viewport.c -o test_viewport
./test_viewport
python verify_r235_geometry.py /path/to/PSC_R235_NATURAL_PLAYTHROUGH_VISUAL_QA_20261121_V2.zip
```

## Required integration before calling it playable

1. Verify that the build sources are **the authentic R235 engine sources**. The historical `C:\\gba\\ps4gba-port` C++ tree is not proven to produce the accepted R235 ROM; the published repository is an older R48 source research archive. Do not plug these functions into either and claim it is R235.
2. Integrate camera and clipping into the real field renderer, call camera reset on scene transitions, and retain original world coordinates, collision and story logic.
3. Integrate the new UI rectangles with real battle/dialogue drawing and cursor placement, validating menus, portraits, sprites, effects, save/load and scrolling.
4. Offer an opt-in runtime toggle against stock R235; preserve 32 MiB, GBA Micro timing, no framebuffer downscale, no fast-EWRAM hack.
5. Replay matched inputs on mGBA and VBA-M/Delta and test on GBA hardware before promotion.

The earlier `PSC_R235_GENESIS_CROP_EXPERIMENT.gba` is only an embedded **static comparison mode**. This branch does not falsely describe it as a live-game renderer.

## Verified live-game injection (2026-11-22)

The repository also contains `r235_field_hook_probe.S`, `build_r235_field_hook_probe.py`, and `verify_r235_live_field_hook.py`. These implement a guarded **no-op** relocation of R235's known active ARM field-camera hook, using retired PCM sample-42 space reserved in the R234 free-space map. R235's exact 124-byte camera shim and hook match R86's corresponding source and binary. The replacement preserves camera coordinates while establishing a confirmed live-game insertion point.

A 32 MiB canary ROM was compiled against verified accepted R235 SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`. Candidate SHA-256: `da48301d7285822dc0306be174e87221dd744bd3b8843b2e7a3dd80f1880feea`.

The Academy live-field mGBA replay matched **11/11 screenshot CRCs**, map IDs and positions, including a room transition. CPU profiling showed the retired original camera body stopped executing and the new body executed. Three-core **launch** checks also matched baseline. See `R235_LIVE_HOOK_TEST_RESULTS.md` for exact addresses, limitations, and the next work item.

**Do not mistake the canary for the promised visible PSII/PSIII-style renderer.** Changing the field camera requires tracing BG map presentation and OBJ drawing together. The current R235 Academy already shows 1:1 original-size graphics.

## Stage 2: verified live Academy BG scroll ownership and battle regression

- `R235_FIELD_BG_DMA_TRACE.md` documents the exact executed ARM DMA0 setup addresses and EWRAM double buffers controlling eight BG horizontal/vertical registers per scanline, using an R235 Academy save state. `trace_r235_bg_dma.py` independently reproduces the read-only evidence from the exact accepted ROM.
- `R235_BATTLE_THREECORE_CANARY_QA.md` and `verify_r235_battle_threecore.py` document baseline-vs-canary sprite/HUD parity across **mGBA, VBA-M, and the Delta-style core** in a real Zio battle staged by a QA-only encounter driver. No battle UI has been changed yet.
- A **diagnostic-only** test adding 8 to the actor camera argument changed **885 of 38,400** screen pixels after the same Academy movement while retaining player world coordinates. Since background was unchanged, this is evidence that a separate BG update is necessary, **not** a usable native-pixel alternate rendering mode.
- The accepted game already preserves full native tile art in this Academy scene, so blanket downscaling is not an acceptable experimental solution.

**Remaining implementation:** identify the BG scroll-buffer producer, synchronize BG/OBJ framing in the live engine, introduce a safe opt-in, and implement/validate dynamic dialogue and battle menus (and complete gameplay regression). The no-op field-hook canary is still the only safe experimental playable ROM at this checkpoint.

## Stage 4 — live coordinated field camera: VERIFIED IN THREE CORES

The first **visible**, synchronized background + OBJ camera experiment is now source-reproducible under [live-camera/](live-camera/). It pans the native field crop by X=16, Y=8 without resampling graphics. Test R235 ROM SHA256: `f3a80bad91b6d3f1809bf1e09ea09f887e204428aeb55247bf67426caf03f89d`; parent accepted R235 SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.

**Measured**: real PSIV new game opening field map 94 was tested in mGBA, VBA-M libretro and Delta-style libretro, with four field checkpoints each showing **100% exact pixel match after cropping the original 16px from left and 8px from top**, while all map/player state remained equal. The original title screen was unaffected. Three Zio battle command checkpoints per core matched unchanged R235. The map18 Academy saved-state route also passed nine scene samples in mGBA.

The separate fixed-offset cartridge is intentionally experimental, with no title menu toggle and no edits to the accepted ROM. It is NOT a completed PSII/PSIII-style full game conversion. Scene-edge camera clamping, dialogue layout, battle HUD layout, and full playthrough remain unfinished. The hold-R preview mode was rejected for now because stationary scenes can retain stale framing after the button is released. See [the detailed test record](live-camera/VERIFICATION_20261122.md) and [battle HUD source audit](BATTLE_HUD_SOURCE_TRACE_20261122.md). No commercial ROM or extracted assets were committed.

## Stage 8 — battle backdrop preference UPDATED

The user explicitly prefers accepted R235's **full scenic battle compositions** over the Stage6/7 240x160 centered Genesis-original crops. The centered source-native crop hides important environmental details (especially aurora/sky in the purple-and-green Rykros-type backdrop). **Do not reintroduce globally cropped battle backdrops** during subsequent art improvements. Preserve atmospheric/sky features and the original R235 backdrop layout; prioritize original pixel detail on enemies, party poses, field sprites, portraits, manga and UI instead.

A separate **Stage8 experimental ROM** restores all 13 replaced battle backdrop records from accepted R235 while preserving Stage7's static enemy detail and 40 manga panels, muted portrait border and field camera. SHA256: `7d5afd0b0472542a700aa6af8e8c46dc7a86115cf18ad3567d4bb5f753b08fff`. In three emulators, nine live battle command snapshots from backdrop #0 matched accepted R235 exactly; 27 launch/field/UI checkpoints matched Stage7. Backdrop #21 was also restored and its indexed pixels decoded to exact R235 data, but its actual Rykros encounter has **not** been played through. See [Stage8 QA and source](artwork/STAGE8_SCENIC_BACKDROP_PREFERENCE_QA.md).


### Stage 9 — current preferred experimental ROM

The current test ROM combines the Stage8 **accepted R235 complete-scene battle backdrops** (do not crop away the Rykros sky), the muted portrait borders, 40 manga, Stage4 field crop, and original-source static detail for four enemies **0x06, 0x10, 0x09, 0x0C**. SHA256 `53848793971f0fe77c6880560807816652573925c73467f162c4835677ff558c`. Stage9 added only 392 static sprite bytes; 24 single-enemy battle checkpoints plus 27 field/UI regression checkpoints passed across mGBA/VBA-M/Delta-style cores. See [Stage9 engineering record](artwork/STAGE9_NATIVE_ENEMY_DETAIL_QA.md). It remains experimental, not a full-game FIN or real-hardware acceptance.
