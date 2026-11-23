# R235 native viewport experiment: battle canary parity

Date 2026-11-22 Eastern. The authentic R235 accepted ROM (SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`) and the branch's *no-op* relocated field-hook canary (SHA-256 `da48301d7285822dc0306be174e87221dd744bd3b8843b2e7a3dd80f1880feea`) were independently loaded into three emulation cores.

A temporary test-only native context driver staged an actual **Zio battle**. In each core, baseline vs canary rendered identical RGB screenshots, frame counts and battle-state values at:

| Core | command (frame 1845) | command submenu (frame 1858) | selected (frame 1871) |
|---|---|---|---|
| mGBA | `b26139e9` | `12eb9d58` | `ebf08557` |
| VBA-M libretro | `264bca28` | `3c83279c` | `8aae7505` |
| Delta-style VBA-M | `264bca28` | `3c83279c` | `8aae7505` |

These are screenshot **CRC32s**. Differences between emulator families reflect their rendered colors; the baseline/canary match **within** each emulator is the test criterion. Each battle stayed in state 6 and output native 240×160 pixels. The original ROM was never overwritten, and the temporary encounter driver is not a production ROM patch.

The Academy field and Zio battle use different rendering paths: field uses Mode 0 GBA tile BG/OBJ; Zio screenshot shows battle foreground/background and command/status surfaces. The field-camera shim alone must **not** be used to reposition battle menus.

**Status:** Field hook injection verified; three-core Zio battle unaffected. **Not yet implemented:** observable alternate camera framing, live BG/OBJ camera integration, dialogue reflow, battle HUD reflow, or complete-game QA. Next: trace the field BG/map-compositor's logical camera origin and match actor OAM positioning; then opt-in camera-mode implementation. Battle UI requires a separate verified source hook and a game-mode gate.
