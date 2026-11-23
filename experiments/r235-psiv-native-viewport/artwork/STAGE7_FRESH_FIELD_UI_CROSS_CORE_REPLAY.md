# Stage 7 independent cross-core field/UI replay — 2026-11-22 23:21 ET

This checkpoint documents a fresh execution in the current run, not a conclusion from filenames.

## Inputs independently checked
- Accepted R235: 33,554,432 bytes; SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`; immutable parent reference.
- Experimental Stage 7 candidate (two enemy static tile banks only): SHA-256 `edf7b67c1c651fed51a12f248aa560c191b3ba0cf70f003fb71d7136b0f645be`; 33,554,432 bytes.
- Stage 6 test parent for this candidate is the silver-border variant, not the accepted R235. Do **not** mislabel this as a direct accepted-R235 patch.
- Repro code: local `stage7_work/field_ui_regression.py`; emulator libraries from PSC headless toolkit. Original ROM/assets and binaries remain local, not committed.

## Newly executed real-emulator regression
Re-ran the field/UI comparison script on both the Stage 6 parent and Stage 7 candidate, launching PSIV and capturing title, new-game selection, confirms, and early field at nine deterministic checkpoints per core. Framebuffer CRC, frame index, map ID, and player coordinates compared for each checkpoint.

- mGBA: **9/9 identical**
- VBA-M libretro: **9/9 identical**
- Delta-style VBA-M core: **9/9 identical**
- **27/27** exact checkpoint comparisons passed.

This is a valid negative-regression test of the specific early field/UI route, **not** validation of dialogue portraits, native 1:1 viewport framing, battle UI, or complete gameplay. Stage 7 live enemy tests are documented separately; do not infer full animation correctness.

## Next evidenced blocker
The existing three save-state windows never show a portrait; therefore their zero-difference border results provide no dialogue coverage. Build a deterministic pre-dialogue replay or obtain a verified active-portrait save state, capture actual tile/OBJ/border framebuffer ROI in both mGBA and VBA-M, then compare original Genesis border pixels. Do not promote the optional silver border or claim native viewport acceptance until this passes. Production R235/R236 and main remain unchanged.
