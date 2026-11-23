# Stage 7 — field-safe Genesis enemy sprite detail and non-sand battle comparisons

**November 22, 2026 (America/New_York). Experimental branch only.**
Source used: user-owned original US PSIV ROM SHA-256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`; archived PSIV graphics reference code `PSC_R203_COMPLETE.zip` SHA-256 `46c39d7f771a645a92357641572ac3820fe01a3690a2cad76cb78f17b1b1dd46`. None of these ROMs or extracted images are in the public repository.

## Backdrop QA (more varied than Motavia)
Decoded all 13 indexed backdrops newly restored in Stage 6: **0, 2, 4, 5, 16, 17, 20–26**. Exact, RGB555 pixel-for-pixel equality between each Stage 6 240×160 decoded backdrop and its matching unscaled Genesis crop (40,32) is confirmed. The original R235 version visibly differs. Created a 13-row 3-column source/current/experimental comparison and a more legible 4-scene comparison: snowy mountain #4, stone interior #17, alien green/purple landscape #21, violet wall #24. These are **decoded asset comparisons**, not claims about all live battles.

## Enemy sprite audit
Audited all 153 enemy records against US Genesis ROM with the R198 `audit_enemy_bank.py` from the user source archive:
- **152 / 153 palettes** match source exactly.
- **148 / 153 source tile maps** match exactly.
- **153 / 153 animation banks** have byte differences; 126 / 153 body banks differ in at least one byte.
- A tile-byte mismatch does **not** independently establish a bug: projection optimizations and animation handling matter, especially giant special bosses. Do not apply a blanket restore to all 153!

## Carefully scoped live sprite experiment
From the fully verified Stage6 ROM `e8d5418560cdd43fac46a6e5214b7adc691c17b8f4e3bc1eddb58741a77f57b0`:
- Enemy **0x06**, native 64×48, source palette/map exact, restored 15 static body tiles / **185 bytes** changed.
- Enemy **0x10**, native 80×88, source palette/map exact, restored 75 static body tiles / **1230 bytes** changed.
- Retained every Genesis-defined animation-destination tile **byte-identical** to the parent; no AI, event, sprite dimensions, pointers, palette, map, battle HUD or GBA header bytes were modified.
- Candidate ROM: SHA-256 `edf7b67c1c651fed51a12f248aa560c191b3ba0cf70f003fb71d7136b0f645be`, CRC32 `2781c796`, exactly 32 MiB, **1415 changed bytes** in two known enemy static body banks.
- Independently rebuilt from archived decompressor and sources; exact SHA and whole-ROM binary comparison passed.

## Real emulator tests
Using native debug single-enemy encounter fixtures:
- Enemy 0x06, four states per core, mGBA / VBA-M libretro / Delta-style VBA-M: **PASS**. Exactly **254** changed screen pixels per state, inside enemy bbox x97–142, y62–85. Bottom HUD (y116–159) pixel-identical.
- Enemy 0x10, four states per core: **PASS**. Exactly **916** changed pixels per state, inside bbox x90–149, y29–85. Bottom HUD pixel-identical.
- Battle frames, command states and selected commands retained baseline state values at all sampled checkpoints; both enemy sprites visibly redraw from new artwork.
- Separate original vs candidate **9-checkpoint title/cutscene/start/field path** in each of mGBA/VBA-M/Delta-style: **27/27 exact screenshot CRC/map/player/frame parity**, so no detected field or UI regression from these enemy edits.

## Other graphics: party, overworld, UI
- Extracted source-authentic original **11 party members' idle/cast rear-view battle poses**, plus original Chaz/Hahn/Rika attacks, from the verified Genesis source. These are ready for comparing against real R235 battle animation frames. **No party art changes yet**.
- Source `source/field68.c` and `source/field74.c` from R86 show native tile-cache publication and deferred object uploads. The accepted Academy field already preserves source 1:1 tile geometry. **Overworld sprite conversion has not yet been implemented**.
- Historical `r191/src/windows.c` hardcodes 240×160 GBA window bounds. R235's active Zio HUD Thumb text positions are known from previous stage; **no new UI layout change** in Stage7. Maintain no changes to portrait border preference except the accepted optional muted Stage6 choice.

## Limits / next steps
This is an opt-in isolated sprite demonstration, not an accepted R235 release. Four battle checkpoints do not establish full attack/defeat animation fidelity, all enemy forms, all mapped field scenes, or a complete playthrough to FIN. No physical GBA/Micro test was performed. The Stage6 camera transition, manga composition, selected backdrop encounter and enemy idle/attack timing issues remain unclosed. Move next to party battle pose comparison in real battles, evaluate per-enemy active sprite projection at animation boundaries, and inspect real field object OAM and UI border/cursor mapping before altering them.

No copyrighted ROM bytes or art are published to GitHub.