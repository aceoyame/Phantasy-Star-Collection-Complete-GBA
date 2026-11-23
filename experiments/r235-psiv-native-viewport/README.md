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
