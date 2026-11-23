# Stage 10 extended battle regression — 2026-11-23

New execution: 36 actual framebuffer checkpoints, **6 offsets × 2 enemy contexts (#09, #12) × 3 cores (mGBA, VBA-M libretro, Delta-style VBA-M)**. Each emulator was launched with the real 32 MiB Stage9 and Stage10 ROMs, injected with the same verified slot-149 save, navigated into an actual battle using the existing QA context driver, and advanced to offsets 0, 30, 60, 120, 240, 480 frames.

- Accepted R235 baseline verified locally: SHA-256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- Stage10 candidate verified: SHA-256 `d08d6c88b5c6f6dee2ae15925864e5c98d58b113859d063476ac86ea97161d56`.
- At each of the 36 pairs, emulator frame count and battle-state byte `0x02028d04` matched.
- All 36 screenshot comparisons **PASS**: no changed pixels above y=84 or below y=128; battle backdrop, enemy and HUD region unaffected by the three party sprite palette-index corrections.
- This is a meaningful longer idle-battle timing test, **not** proof that casting/attack animations work, and **not** an end-to-end playthrough or original Genesis pixel-identity check.
- No new ROM patch was made; the existing Stage10 test build remains the current candidate, not accepted production.
- Actual script and JSON results were generated locally as `stage10_extended_battle_regression.py` and `stage10_extended_battle_results.json`. They are not copied to public GitHub because the run artifacts include local paths and source tool details.

Next blocker: obtain a deterministic spell-cast/attack input replay that visibly exercises both altered cast frames, then validate original Genesis effect alignment and state timing across mGBA and VBA-M before expanding to other party characters. Preserve the original full-composition R235 scenic backdrops including Rykros auroras.