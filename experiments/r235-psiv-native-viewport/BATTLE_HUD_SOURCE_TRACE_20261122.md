# R235 battle interface trace — next integration target

Evidence collected against accepted R235 SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`, using a temporary local Zio encounter driver and native mGBA profiler. No original ROM is modified or uploaded.

The historical R197 battle HUD source identified Thumb text offsets at ROM addresses:
- `0x0883D42C`: `adds r1, #14`, HP/name text Y offset
- `0x0883D440`: `adds r1, #14`
- `0x0883D44E`: `adds r1, #22`, TP/status text Y offset
- `0x0883D45E`: `adds r1, #22`

These bytes are still present in R235. **Active code confirmed**: executing `0x0883D42C` and `0x0883D44E` both hit while entering Zio battle command phase at frame 1760, game battle state 6. The other two offsets are present but were not separately execution-tested.

The historical generic `window_box` patch at `0x08827D34` and legacy ARM HUD text addition at `0x08805FD0` **were not hit** during the same battle command entry test up to frame 1875. That absence is limited to this scene and window.

**Implication:** The active R235 battle HUD uses the inherited Thumb text layout path. Do NOT modify the generic R191 `window_box` routine expecting it to relocate battle commands. Need identify the active battle-border and cursor placement paths; adjust them and the text offsets as a unit, then test in Zio, DF1/2/3 and PD3 before calling any layout change finished. Do not relocate font glyphs alone and create clipping.

The historical `r194/src/battle_render.c` uses 240-pixel rows for GBA battle shadow and an 86-row effects band, evidence that the existing native battle renderer already handles GBA-sized pixel data rather than globally shrinking a Genesis framebuffer. This does not prove every attack or enemy is identical to original.

Field pan on this branch is a separate verified experiment and must not leak into battle UI.
