# R235 PSIV native-pixel crop — real emulator verification

**Candidate is an experimental *fixed field-camera offset*, not the completed PSII/PSIII-style alternate UI.**
No game ROM or extracted assets are committed to Git.

## Binary identity and ROM safety
- Parent: `PSC_R235_FIXB_NEWGAME_RELEASE.gba`, 33,554,432 bytes, SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`
- Result: `PSC_R235_COORDINATED_BG_OBJ_PAN16X8Y_TEST.gba`, 33,554,432 bytes, SHA256 `f3a80bad91b6d3f1809bf1e09ea09f887e204428aeb55247bf67426caf03f89d`, CRC32 `50ce830c`.
- Exactly 193 bytes changed in four bounded regions: BG writer branch at `08C4D198`, actor hook at `08CD3854`, code at `08F97D64` (132 bytes) and `08F97F00` (60 bytes).
- Code ranges lie entirely inside R234's verified Tier-B retired sound sample 42 space. Source/ROM header bytes preserved. Standard GBA/Micro addressing and timing retained, no faster-EWRAM workaround.
- Both the field-actor camera and native BG raster-scroll row fill are shifted by X=16, Y=8. No screen resampling, no image downscaling.

## Authentic live field tests

GBA R235 after-principal Academy savestate: 9 input checkpoints (idle, RIGHT, LEFT, UP), every world map ID and player pixel coordinate matched original R235. After startup settling, **all 8 cropped overlap comparisons scored 1.000000** (`candidate[:152,:224] == original[8:,16:]`). This is **pixel-exact**, including background, player and NPC sprites.

Natural new-game field route: the launcher enters PSIV; at frame **4039**, map ID **94**, world player coordinate **(408,476)**. Recorded field at 4039; scripted movement at frames 4044, 4054, and static at 4069. Against the original R235, the candidate has a **1.000000 overlap ratio** at each of those four samples in all three cores:
- mGBA — PASS
- VBA-M libretro — PASS
- VBA-M Delta-style libretro — PASS

The Phantasy Star IV title is pixel-identical to R235 in all cores. At every checkpoint, map ID, player coordinates and frame counters agree with original R235.

## Battle regression

The actual Zio debug battle was run using a temporary operator-local debug fixture. **Three screenshots per core** (command, commands open, selected) match accepted R235 frame-by-frame and in battle state. mGBA, VBA-M, and Delta-style cores each PASS. This confirms the field offset has not changed that tested battle sequence, **not** that any new battle UI is implemented.

## Unresolved / next implementation

- The fixed pan is visibly a different crop but is not a camera-follow redesign; camera dead-zone, clipping at corners, map transitions, dialogue windows, and battle HUD layout still need dedicated tests and changes.
- A hold-R interactive variant correctly matches the shifted field during movement and reverts after more redraws, but sometimes fails to redraw immediately when stationary. **Do not promote that variant**.
- Need test scene-edge scroll, larger sprites, animation, dialogue/UI, warps and battle transitions, including physical GBA/Micro, before release-candidate status.
- No manual claims of a complete playthrough or ability to reach FIN; this testing is local to the listed scenes.

## Build and test

```sh
python build_r235_native_crop.py /path/to/PSC_R235_FIXB_NEWGAME_RELEASE.gba /path/to/experimental.gba
```

Use the user's headless toolkit and accepted save states to run Academy and Zio replays. No ROM or copyrighted Sega assets belong in the public repository.
