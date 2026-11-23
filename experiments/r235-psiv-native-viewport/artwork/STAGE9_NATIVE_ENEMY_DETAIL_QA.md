# Stage 9 — four improved enemies, full-scene original R235 battle backgrounds

2026-11-22/23, Eastern time. This update builds on [Stage 8](STAGE8_SCENIC_BACKDROP_PREFERENCE_QA.md) and **keeps the user's preferred R235 scenic compositions and aurora/sky detail**. Do not restore the overly tight Stage6/Stage7 native background crop.

## Exact ROMs
- Stage8 parent: SHA256 `7d5afd0b0472542a700aa6af8e8c46dc7a86115cf18ad3567d4bb5f753b08fff`
- Stage9 test candidate: SHA256 **`53848793971f0fe77c6880560807816652573925c73467f162c4835677ff558c`**; CRC32 `a8d860f5`; 33,554,432 bytes
- Original Genesis US reference: SHA256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`
- Accepted R235 remains `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308` and unchanged.

## Actual modifications
Only 392 new bytes beyond Stage8, within the existing static-body tile banks of **enemy 0x09 and 0x0C**. Palette and map were source-verified byte-identical before patching. Animation-destination tiles are strictly protected:
- Enemy `0x09`: 92 static tile bytes, six tile records; source-native 48×48.
- Enemy `0x0C`: 300 static tile bytes, 18 tile records; source-native 80×80.

Already-retained Stage7 source-detail enemies **0x06 and 0x10** remain. The 40 manga, 39 quieter portrait borders, native field-camera experiment, and *all original R235 battle backdrops* are unchanged.

## Real emulator evidence
Stage8 vs Stage9 true single-enemy debug encounters in **mGBA, VBA-M, and Delta-style VBA-M**, four command/idle checkpoints each (24 total):
- `0x09` 95 changed output pixels per sampled frame, solely x103–136/y69–85.
- `0x0C` 441 changed pixels per frame, solely x92–147/y34–73.
- Frame counters and sampled battle-state bytes match; bottom party HUD and battle backdrop are pixel-identical to Stage8.
- Independent nine title/launch/field UI checkpoints per core also match Stage8 exactly — **27/27**.

Stage8 backdrop tests remain relevant: all **13/13** changed Battle backdrop indices decode pixel-identically to accepted R235 and the real *Motavia* indexed-backdrop-zero debug battle renders pixel-identically to accepted R235 in three cores. Rykros-index-21 source artwork is restored but **no complete natural Rykros battle has been replayed**. A temporary register-injection attempt did not successfully force index21 on screen and is not accepted as a successful Rykros live test.

## Next graphic work
Proceed through real party battle poses, attack/cast animations (11 members), other enemy idle/attack/death frames, active field OAM/OBJ sprites, window borders/cursors and menus, remaining manga compositions, vehicle battles and effects. Evaluate **full scenic composition first**: keep skies and auroras. Never claim a generic original-resolution crop is better when it removes source visual content.

This ROM has **not** passed a full FIN playthrough, real GBA/GBA Micro testing or exhaustive battle animation parity. No Sega ROM bytes/images should be committed to public GitHub. The user-facing Stage9 private QA kit contains screenshots, ROM hashes, build steps, and emulator comparisons.

Reproducible, SHA-pinned source-only builder: [build_stage9_source_enemy_detail.py](build_stage9_source_enemy_detail.py).
