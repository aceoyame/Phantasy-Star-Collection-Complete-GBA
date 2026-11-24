# Stage 27 — R238 integration regression isolation (2026-11-23)

**Diagnostic-only checkpoint. No playable ROM promoted.** The R238 Stage26 candidate remains SHA256 `9d9ca1b32cfdd844d648fe00caa9549bd702c193d4753d7de9dc5cadd2b365fa`; Stage26 parent `3154fb729fdb016574d63dff8dea84e01f1b77e2ac346390530d4f90d932a1a0`. All work used actual Library inputs and headless emulator toolkit. Mainline remains separate.

## Field timing and audio: isolated cause

The R238 source hook at 0x08866F84 replaces the original BL to 0x088676EC with the fog reveal wrapper. Exact single-hook restoration in a private diagnostic ROM yields:

| Core | R238 candidate | No-fog diagnostic | Fast-gate diagnostic |
|---|---:|---:|---:|
| mGBA | 63/69 | 69/69 | 63/69 |
| VBA-M | 68/69 | 69/69 | 69/69 |
| Delta-style VBA-M | 57/69 | 69/69 | 57/69 |

Each replay is 3135 frames. All three no-fog diagnostics have **bit-identical PCM to Stage26**. The fast-gate diagnostic still has nonidentical PCM in all three cores. These are exact frame comparisons, not shifted phase matches. The bypass disables fog discovery and is not a release solution. The short ARM gate at 0x09250200 checks big-endian map ID at 0x0200EC28 and skips the fog body on indoor maps, but even that overhead fails timing parity. It is rejected pending a verified world-only producer. Preserve fog, do not disable audio or skip frames.

At the current hook, the caller branches based on 0x0300634C and 0x02029FB8; a second original call to the same function exists at 0x08867018. Do not assume this call site alone is a general once-per-field-frame update. Trace world-specific execution and demonstrate discovery on actual world movement before changing production code.

## Battle sprite and targeting regression

Replayed the previously verified Stage14 fixture for actual Zoran Bult (ID 0A), Gicefalgue (0B), and Xe-A-Thoul (7B x3). 36 paired checkpoints per tested pair (three cores x three formations x four checkpoints); enemy IDs, battle state, party/HUD matched on all. R238 candidate differs on 34/36 frames in enemy pixels only, with identical observed difference counts across the three cores; examples in mGBA: Zoran 52–284, Gicefalgue 36–116, Xe-A-Thoul 12–18 changed pixels. R237 vs traditional R238 reproduces the same difference counts, bounds and pattern (34/36), so it is not exclusive to experimental relocation. Temporarily restoring the two enemy loader call sites at 0x0882FD68/0x0882FDB8 or the fog hook alone does not fix the battle discrepancy. Do not paint over the sprite data to compensate. Need per-frame enemy pose/palette/tick trace and a fresh original-source comparison.

Xe-A-Thoul target selection matched **18/18 checkpoints**, including marker centers 54 -> 120 -> 186 -> 120 in every core. No change needed there. These are controlled native battle fixtures, not a natural story playthrough or FIN acceptance.

## Reproduction and next actions

Run `python3 tools/r238/make_stage27_diagnostics.py /private/R237.gba /private/R238_STAGE26.gba /private/probes` to regenerate the exact three diagnostic ROMs. No commercial data is embedded. Use the Library headless toolkit and Stage14 QA fixture, compare parent/variant with the same input and core, and preserve every checkpoint. The user-only `PSC_R238_STAGE27_DIAGNOSTICS.zip` contains raw reports, replay scripts, screenshots, and rebuild script. Do not redistribute diagnostic ROMs or claim they are promoted.

Next priority: world-specific fog producer trace and actual map-discovery regression; then enemy pose/palette timing trace, keeping original scenic battle backgrounds and Stage26 sprite art intact. No full FIN run or hardware validation has been performed.
