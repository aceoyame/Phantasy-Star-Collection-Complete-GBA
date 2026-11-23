# Stage 7 overworld sprite and UI source-backed audit

This supplements [Stage7 enemy and party findings](STAGE7_ENEMIES_PARTY_FIELD_UI_QA.md). It is **read-only**; no claimed overworld/UI artwork replacement.

## Exact source authority

Historical field publisher `source/field68.c` / `source/field74.c` in user-held `PSC_R86_CUMULATIVE_HANDOFF_20261103.zip`, with symbol addresses from `support/PSC_R37_TEST_20261026/tests/native_symbols.json`:
- `field_objs`: GBA EWRAM `0x02026DAC`, 32 records × 32 bytes.
- `field_obj_count`: GBA EWRAM `0x02024110`.
- Per-object fields: type(+0), tile(+4), world x/y(+6,+8), active(+10), animation(+11), GBA tile(+26), **pixel width/height(+28,+29)**, local offsets(+30,+31).
- Original R86 field source explicitly defers VRAM publication of active sprite tiles and guards drawing on valid dimensions; this is not evidence that sprite pixels have been rescaled.

## Actual mGBA evidence on accepted Stage6 and new Stage7

**Academy after-principal settled savestate:** map18, 3 active object records. Slot0 type `0x006C` uses a native **16×32-pixel** sprite, GBA tile488, animation 3, screen offset (0,-2). Slots1/2 type `0x0074` have width=height=0 at this checkpoint; they cannot be called rendered sprites from those records. Field OAM bytes and whole-screen CRC `6f6e85fe` matched Stage6 versus Stage7. No world position/state differences detected.

**Natural new-game opening field:** map94, player native position (408,476); 9 object slots in memory, none marked active at the chosen screenshot. Full-screen CRC `da2ee1fd` identical between Stage6 and Stage7.

The current 16×32 native sprite record suggests at least this field character is already rendered at a GBA-native pixel size, rather than being globally scaled. To improve overworld sprite fidelity without regressions, align original Genesis NPC sprite tiles/poses to live actor types and their real VRAM/OAM mapping first; use active objects in multiple cities and battle/vehicle transition checkpoints. Do not blindly resample all field sprites.

## UI guard

At the two Stage7 single-enemy live battles, **all changed pixels are inside the enemy bounds**, with no HUD changes at y>=116. At 9 launch/field checkpoints × 3 cores, screenshot CRCs, player coordinates, map IDs and frame counts remain identical to Stage6. R191 `src/windows.c` in the R203 archive uses native GBA bounds (240×160) already. Distinct menu font, border, command cursor and battle window fidelity still require original-Genesis comparisons and live scene tests. This is a regression safeguard, not a UI redesign.

**Do not treat Stage7 as overworld/UI completion**; the source-authenticated audit and repeatable checkpoints are the current frontier.
