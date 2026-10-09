# PSIV PSCSTYLE v0.6 — combat stat-sync fix + field investigation (Claude, 2026-09-24)

Follow-up to `PS4-PSCSTYLE-v0.5-BATTLE-ANIMATION-HANDOFF-2026-09-24.md`. Same
environment constraints as v0.4/v0.5 (no ARM toolchain, no emulator) — see
those docs for what that does and doesn't let me verify.

## Fixed: melee (and effectively all combat stats) always dealt ~1 damage

This was a real, single root cause, not a damage-formula bug. I checked the
formula itself first — `battle_calc_damage_ex()` — against
`Battle_CalculateDamage` ($B5C8) instruction for instruction, and it's exact.
I then checked every input to it byte-for-byte against the disassembly:
item struct stride (22 bytes, `Battle_GetItemOffset`), the element-byte
offset (+18, `Battle_LoadWpnAttackElem`'s `InventoryData+$12`), the
item-type offset (+10, `Battle_GetItemType`), the `PS4_ITEM_DATA` base
address itself (`0x2A8E28`, matches `InventoryData`'s assembled address in
`ps4.lst` exactly), and the stat field offsets
(`atk_pow_battle=$26`/`dfs_pow_battle=$2A`/`magic_dfs_battle=$2E`, straight
from `ps4.constants.asm`). Every one of them was already correct.

The actual bug: PSIV's character stats live in three RAM tiers per stat —
raw/leveled, "_mod" (raw + equipment, kept current by the port's
`update_one_char_derived()`), and "_battle" (the live value combat actually
reads). The source has a dedicated routine, `FillBattleStats` ($7DDA),
called exactly once, from the generic battle-entry setup that runs before
*every* battle (random or scripted) — it copies "_mod" into "_battle" for
all 11 characters unconditionally. This port never called it. The only
place anything copied "_mod" into "_battle" was inside
`field_recover_party()`, reachable from exactly two places (a scripted
Recovery event, and sleeping at Chaz's house) — never battle entry, never a
level-up, never an equip change. So on a fresh save every "_battle" field
starts at 0 and stays there indefinitely; `battle_load_char()`'s own
`if(!a->atk)a->atk=1u;` safety clamp then means every physical attack's
formula runs with `atk` pinned to 1, and the result always rounds down to
the clamped minimum of 1 after subtracting the target's defense — for
every character, every weapon, the entire game, until you happen to trigger
one of those two specific events.

It's worse than just weak attacks, too: `dfs_pow_battle`/`magic_dfs_battle`
(the party's own defense) were stuck at 0 the same way, so **the party was
taking full, unmitigated enemy damage the whole time as well.** And
`agility_battle` being stuck at 0 means `battle_try_escape()`'s
`if(!highest||...)return 0;` guard made RUN **always fail** — see below.

### The fix

Added `battle_fill_stats()`, a direct port of `FillBattleStats`, and call it
at the top of both `battle_run_event()` and `battle_run_random()`, before
`battle_live_party()` reads any character's stats into the local combat
struct:

```c
static void battle_fill_stats(void){
    for(unsigned ci=0u;ci<11u;ci++){
        uint16_t d=(uint16_t)(0xF500u+ci*0x80u);
        ram8w((uint16_t)(d+0x1Au),ram8r((uint16_t)(d+0x19u)));
        ram8w((uint16_t)(d+0x1Du),ram8r((uint16_t)(d+0x1Cu)));
        ram8w((uint16_t)(d+0x20u),ram8r((uint16_t)(d+0x1Fu)));
        ram8w((uint16_t)(d+0x23u),ram8r((uint16_t)(d+0x22u)));
        ram16w((uint16_t)(d+0x26u),ram16r((uint16_t)(d+0x24u)));
        ram16w((uint16_t)(d+0x2Au),ram16r((uint16_t)(d+0x28u)));
        ram16w((uint16_t)(d+0x2Eu),ram16r((uint16_t)(d+0x2Cu)));
    }
}
```

### Verification

- New test `tests_pscstyle_v06_battle_fill_stats.py` (added to
  `scripts/validate_current.sh`): pulls the seven mod->battle offset pairs
  directly out of `references/disassembly/ps4disasm/ps4.constants.asm`
  (not retyped by hand), confirms `battle_fill_stats()` copies exactly
  those offsets for exactly 11 character slots using the same
  `0xF500+ci*0x80` base `battle_load_char()` uses, and confirms both
  battle-entry functions call it before `battle_live_party()`.
- All prior gates (12 from v0.5, plus this one) still pass.
- Host `gcc -fsyntax-only -std=c11 -Wall -Wextra` still reports zero new
  warnings for the changed functions.
- **Not verified:** actual in-game damage numbers. I don't have an
  emulator here. Please build, start a fresh file, and confirm a level-1
  Chaz's basic attack does noticeably more than 1 damage to an early
  enemy, and that an enemy's attack against your party is not doing its
  full unmitigated roll every time.

## RUN (escape) — fixed as a side effect, no separate change needed

`battle_try_escape()` uses `party[i].agility` (== `agility_battle`) to find
the party's best escape chance; with that field stuck at 0 pre-fix,
`highest` was always 0 and the function's own
`if(!highest||battle_random_escape_param>=0xF0u)return 0;` guard meant RUN
could never succeed, regardless of formation or party agility. I checked
`battle_calc_chance()` (the shared roll used by escape, hit chance, and
several effects) against `Battle_CalculateChances` ($B5A8) instruction for
instruction — exact match, not the source of any bug. This one didn't need
its own fix; `battle_fill_stats()` above resolves it along with the damage
issue, since they share the same root cause.

## DEFEND and ITEM usage — investigated, nothing broken found, not exhaustively proven

**Defend:** the port halves incoming damage while `party[i].defending` is
set (`if(party[victim].defending)dmg=(dmg+1u)>>1;`, applied consistently to
both the basic enemy attack and enemy-skill damage paths). This is
internally consistent and a reasonable, common implementation of a JRPG
Defend command. I looked for the exact source mechanic
(`Character_Defend` at $85CC) to confirm it's a straight damage halving in
the original too, but that routine is mostly animation/state dispatch and I
could not find the specific damage-side consequence within my time budget
here — I did not find a `Defending` status bit or a `dfs_pow_battle`
doubling anywhere in the disassembly either, so I can't rule out the
original doing something more specific (e.g. per-element). I'm flagging
this as unverified rather than either confirmed-correct or confirmed-buggy;
if you notice Defend feeling off in practice, this is where to look first.

**Items:** `battle_execute_cmd()`'s item branch reads effect/power/target/
resist/element from the same verified-correct `PS4_ITEM_DATA` base and
22-byte stride as the equipment-property lookups the melee fix already
confirmed byte-for-byte. The field structure looked internally consistent
against `Effect_DoItem`'s setup code. I did not trace this as exhaustively
as the melee damage path (no equivalently strong bug signal turned up), so
treat this as "structurally checked, not proven" rather than "verified."

## "Two principals in the office" and the basement monster — investigated, not conclusively resolved

I traced both as far as I could without being able to play the build, and
want to be upfront that neither is resolved with the same confidence as the
damage fix above. I did **not** change any code for either of these — I'd
rather hand back precise findings than guess at a fix for something I can't
verify visually.

**Two principals:** two candidate explanations, both plausible, and I
couldn't fully distinguish between them from source alone:

1. *A field NPC duplication.* The Principal's Office map has (at least) two
   independent "manager" routines that each conditionally hide one specific
   NPC on map entry: manager `$1D` hides Professor Holt unless
   `EventFlag_BioPlantEscape` is set, and manager `$1E` hides Hahn if
   `EventFlag_Reunion` is set or `EventFlag_AfterAlysDeath` isn't. I
   confirmed both are implemented in `title_runtime_v06.c` (`case 0x1Du`/
   `case 0x1Eu` in the map-update-id dispatch) and match the source logic
   exactly, flag-for-flag. So if this is a live double-NPC-on-screen bug,
   it isn't in either of those two managers — it would have to be in the
   principal's own placement data or a third mechanism I didn't locate.
2. *A manga/dialogue panel bug.* `HANDOFF_0.9.8_REPORTED_ISSUES.md` (an
   earlier round's handoff, already in your bundle) documents a previously
   -reported bug called "Principal manga showing three compositions
   immediately" — dialogue 33 entry `$17` loads two manga panels (IDs 0 and
   1) back-to-back to build *one* composited image before any text, and an
   earlier build mistook that for two separate story beats. The existing
   `tests_reported_issues_v098.py` gate for this still passes against the
   current source, and reading `pause_panel_action()`/`panel_render_scaled()`
   directly, the panel-stacking logic looks sound to me. But I can't rule
   out a related regression without seeing it on screen.

If you can tell me whether this happens while walking around the room
(explanation 1) or inside a dialogue/portrait box (explanation 2), that
would immediately cut the search space in half for a follow-up pass.

**Basement "monster" NPC:** this is very likely **Igglanova**, a
boss-type guardian in the Piata Academy basement (B2), tied to the same
kidnapped-Professor-Holt story thread (`EventFlag_BioPlantEscape`) I ran
into while investigating the principal's office. A warning line before the
real fight is a plausible, intentional design for a "not yet fightable"
story guardian — common in this genre. What I checked: the map-entry
manager that hides Igglanova and its two invisible blocker objects once
you've fought it (`case 0x04u`, gated on `EventFlag_Igglanova`/flag `$0B`)
matches the source exactly. There's a separate, unrelated movement-AI gate
elsewhere in the port checking flag `$33`
(`EventFlag_IgglanovaZema`) for object type `0x0054`, explicitly commented
"Zema" — that's a *different* Igglanova encounter, in a location called
Zema, not the Piata basement one, so it isn't a flag mix-up affecting what
you're seeing. I did not trace the actual dialogue text/trigger for
touching the Piata Igglanova before the fight is available, so I can't
confirm whether the specific message is right, missing a follow-up, or
firing at the wrong time. If it's doing something clearly wrong (not just
"a monster talks before fighting me," which may be correct) — like the
message repeating when it shouldn't, or the fight never becoming available
— that's the detail that would tell me it's a real bug rather than
intended content.

## Suggested next pass

1. Build + validate locally, start a fresh save, and confirm melee/enemy
   damage numbers look sane and RUN can succeed.
2. Tell me field-vs-cutscene for the "two principals" sighting, and
   whatever's actually wrong (not just unexpected) about the basement
   encounter, so a follow-up pass can go straight at the real bug instead
   of re-deriving this same context.
3. If Defend still feels wrong in practice after the stat-sync fix (which
   changes what "half of" actually means, since defense was reading 0
   before), that's worth a closer look with real numbers to compare against.
