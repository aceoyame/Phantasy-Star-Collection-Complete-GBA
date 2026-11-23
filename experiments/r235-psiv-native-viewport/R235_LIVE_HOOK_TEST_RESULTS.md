# R235 live field renderer hook — verified experiment

Date: 2026-11-22 (America/New_York)
Test parent: `PSC_R235_FIXB_NEWGAME_RELEASE.gba`, SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`, 32 MiB.
Candidate: `PSC_R235_LIVE_FIELD_HOOK_CANARY.gba`, SHA-256 `da48301d7285822dc0306be174e87221dd744bd3b8843b2e7a3dd80f1880feea`, CRC32 `141ecdcb`, 32 MiB.
No commercial ROM or asset bytes are included in this public branch.

## Evidence / exact binary addresses
- Source authority: `PSC_R86_CUMULATIVE_HANDOFF_20261103.zip/source/party86.s` plus `PATCHES_R86.json`.
- Original camera hook: ARM branch at `0x08CD3854`, bytes `a9 99 ff ea`.
- R86 shim: `0x08CB9F00`, full 124 bytes match R235.
- Relocated shim: `0x08F97D64`, within retired PCM 42 region **explicitly certified Tier B** by `FREE_SPACE_MAP_R234.md`.
- Candidate branch: `42 11 0b ea`; relocated code is **byte-identical** to the inherited camera shim.
- Exactly **120 changed bytes** (4-byte branch and 116 nonidentical code-region bytes). Original GBA header unchanged. Caller and return path intact.

## Results
- The new candidate **booted through 2,009 frames** in mGBA, VBA-M libretro, and VBA-M Delta-style libretro. Each respective core's final screenshot CRC and pixel count matched the baseline, but this is a **launch-only** test.
- A **real R235 Academy save state** was loaded in mGBA (map 18, tile 9,29). Inputs: idle45, RIGHT30, idle15, UP30, idle20, LEFT35, idle20, DOWN35, idle20, A2, idle90.
- All **11** image CRCs, map IDs, and player positions matched original R235. The test successfully changed rooms map18 → map17 without divergence.
- CPU instruction sampling after that sequence: original hook entry `0x08CD3854`: 434 weighted cycles on both. Original code `0x08CB9F00`: **124 baseline vs 0 candidate**. Relocated code `0x08F97D64`: **0 baseline vs 124 candidate**. This proves live execution switched branches.
- `psiv_viewport.c` host C11 unit tests passed.

## Current status
**The experiment is a working, non-visual live-game insertion point.**
Do not describe it as a PSII/PSIII-style alternate viewport yet. The current field already uses original-size tile geometry; we must locate and modify the BG/map camera compositor *together* with OBJ positioning, and separately validate battle/dialogue UI. Shifting only OBJ camera arguments would break spatial alignment. Avoid a GBA mode/scale hack.

## Next executable step
Trace active BG map origin and field actor compositing around `0x08CB9374` across a player movement frame. Then integrate a real option in the test branch to choose alternate native-size viewport framing, preserving all camera state and field collisions. Cross-core test at the Academy, field-edge, and scene-transition checkpoints before any release promotion.
