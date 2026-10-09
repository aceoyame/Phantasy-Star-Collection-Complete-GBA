# R46 Genesis battle-effect recorder

`m68k.c`/`md.c` are a compact 68000 interpreter + Mega Drive VDP (no Z80/audio) built as
`libmd.so` (`gcc -O2 -fPIC -shared -o libmd.so m68k.c md.c`). It boots the verified US
PSIV ROM (`references/roms/ps4built.bin`) unmodified.

`mdrec.py` snapshots `Battle_Objects_Memory` at the `rts` of `Battle_RunObjects` ($367B4)
every display tick, plus CRAM, VRAM and every `Sound_Index` ($FF500A) write, and reproduces
`Battle_FillSpriteAttributes` to get the exact sprite pieces each object drew.

`batch_enemy.py` starts event battle $12 (beach background) from a field save, replaces the
loaded `Enemy_Formation_Data` with a single enemy, issues DEFEND for the party, forces the
enemy's `ability` word at `loc_D024` (after the source AI ran) and records the turn.
`clipx.py` turns a recording into a clip (tracks anchored to the fighter they spawned on).
`tools/generate_r46_battle_clips.py` lowers all clips to `gba/r46_battle_clips.c/.h` and
`gba/r46_tile_pool.bin` (stored in the PS II tail hole of the PSC shell by `build_r46.py`).

The party data used for recording is the debug level-60 party read from a GBA save state
(the port keeps the MD RAM layout in `work_ram`).

Paths in the scripts are the workspace paths used when the data was produced; adjust them
for a new checkout.

## Coverage passes (R46 CP02)

* `batch_enemy.py` – the 234 (enemy, ability) pairs of `tools/source_enemy_effect_specs.json`.
* `batch_enemy2.py` – every other pair in the ROM's `EnemyData` tables ($2816BC, 48-byte
  records: normal list +36..+43, forced list +32..+35), which adds the ability-0 normal
  attacks of 124 enemies; the `$FFEE87` boss opening actions (`EnemyInit_DarkForce1/2`,
  `ProfoundDarkness1`, `CarnivorousTree` → clip kind `enemy_intro`), recorded untouched from
  battle start; and Zio's `$FFEE98` stage sequence (`EnemyAttack_Zio`/`Zio2`/`Zio3` overwrite
  `ability` themselves, so each stage is recorded in order and keyed by the resulting ability).
  Recordings stop when `Current_Actor_Index` ($FF4142) changes, so the next actor's objects
  never leak into a clip.
* `batch_party.py` – techs 1-38 and skills 1-54 (recorder-only ROM patch at $9B94/$9BA6 lets
  the stand-in caster use any skill).
* `batch_combo.py` – the 14 combos: the first `ComboSetupData` ($28549C) entry of each is
  assigned to party slots 0..n-1 and, recorder-only, the turn list at $FFEFAC is rewritten at
  the first `loc_27DE7C` call so the participants act consecutively.

Pairs with no clip are true no-ops in the source (Arm Drone / Float Mine $07/$17 idle turns;
Zio3 stages 2 and 5+, which jump straight to `Battle_DoAttackEffect`) or are lowered in C
because they are palette/formation work rather than objects (Ripper $3A / Piercer $3B →
Twin Arms fusion, loc_23C84).
