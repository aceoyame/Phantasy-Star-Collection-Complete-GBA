# PSIV PSCSTYLE v0.5 — battle animation/rendering pass (Claude, 2026-09-24)

Follow-up to `PS4-PSCSTYLE-v0.4-BATTLE-SPEED-HANDOFF-2026-09-23.md`, this time
on animations and character rendering rather than speed, per your note that
skills in particular "don't seem all programmed in yet." Same environment
constraints as v0.4 (no ARM toolchain, no emulator) — see that doc for what
that does and doesn't let me verify. Everything technique/skill-related below
is grounded directly in `references/roms/ps4built.bin` (byte-verified, not
just read off disassembly comments) and in the real `title_runtime_v06.c`.

## tl;dr

You were right. **25 of the 38 in-battle techniques (66%)** — including Zan,
Megid, Deban, Shift, Anti, Rever, and most of the rest of the spell list —
had **zero** presentation when cast: no cast pose, no effect graphic, no
sound, just the numbers changing on the next frame. Only Foi/Gifoi/Nafoi,
Wat, Tsu/Githu/Nathu, and the Res-family heals had real handlers. I fixed
the *presentation gap* (every technique now at least shows a proper cast
pose instead of standing there idle) but did **not** attempt to build the 20
distinct bespoke spell-effect animations that are still missing — that's
real per-technique asset work I can't do blind without a way to see the
result. Full roadmap for that below.

Skills are a smaller, lower-priority version of the same story: only
Vortex/Earth/Vision have a unique effect, but every other skill already
falls back to either the character's real weapon swing or a cast pose — so
nothing there is silently broken, it's just plainer than the original. No
code change needed there this pass.

## How I found this

`gba/title_runtime_v06.c`'s `battle_party_source_technique()` special-cases
exactly 8 conditions (Foi/Gifoi/Nafoi/Wat/Tsu/Githu/Nathu by exact id, plus
one id-range covering Res/Gires/Nares and their "multi"-target variants) and
otherwise `return 0`. Its caller,
`battle_resolve_mixed_round()` (around line 5233), only calls
`battle_present_action()` — the function that draws the actor's pose, the
cast banner, and does the frame present — when that dispatcher's result is
falsy *and* falls into a shared catch-all `else`. Tracing it end to end:
`battle_execute_cmd()` (the function that actually applies damage/heal/
status) never calls any presentation function itself, and the technique
branch inside it (unlike the skill and item branches two lines below it)
never calls any audio cue either. So an uncovered technique's real,
observable behavior was: TP is spent, the number is applied correctly, and
that's it.

I then went to `references/disassembly/ps4disasm/ps4.asm` to find the
*source's* actual technique dispatch (`CharTech_TechObjsOffs`, a per-technique
BattleObj table) to check whether the missing 25 really each need bespoke
art, or whether the original shares templates the way the port's own
Res/Gires/Nares/Sar/Gisar/Nasar family already does. It does share, in three
places (Gelun/Doran, Anti/Rimpa, Rever/Regen), and I also confirmed from the
same table that Ryuka and Hinas were never missing anything — they're
field-only techniques (their own targeting byte's usability nibble says
"outside of battle only," and `battle_fill_ability_list()`'s existing
`rom8(p+2u)&0x10u` filter already keeps them off the in-battle menu, both of
which I checked directly against the ROM bytes, not just the disassembly
text).

## Full technique table (id, name, status)

`C` = has a dedicated C handler in this port already. `F` = now takes the
new generic fallback. Shared entries point at which other id shares the
*same* source BattleObj, per `CharTech_TechObjsOffs` — build one, get both.

```
 1 Foi     C            11 Gizan   F (own)        21 Doran   F (=20 Gelun)   31 Saner  F (own)
 2 Gifoi   C            12 Nazan   F (own)        22 Seals   F (own)         32 Deban  F (own)
 3 Nafoi   C            13 Gra     F (own)        23 Rimit   F (own)         33 Feeve  F (own)
 4 Wat     C            14 Gigra   F (own)        24 Res     C               34 Anti   F (=35 Rimpa)
 5 Giwat   F (own)      15 Nagra   F (own)        25 Gires   C               35 Rimpa  F (=34 Anti)
 6 Nawat   F (own)      16 Megid   F (own)        26 Nares   C               36 Rever  F (=37 Regen)
 7 Tsu     C            17 Brose   F (own)        27 Sar     C (as "RES_MULTI") 37 Regen F (=36 Rever)
 8 Githu   C            18 Vol     F (own)        28 Gisar   C (as "GIRES_MULTI") 38 Arows F (own)
 9 Nathu   C            19 Savol   F (own)        29 Nasar   C (as "NARES_MULTI") 39 Ryuka  field-only, N/A
10 Zan     F (own)      20 Gelun   F (=21 Doran)  30 Shift   F (own)         40 Hinas  field-only, N/A
```

13 covered, 25 fallback (22 of which need a genuinely distinct new visual —
3 pairs share one), 2 not applicable. `Sar`/`Gisar`/`Nasar` really do use
the same `TechObj_Res` as `Res`/`Gires`/`Nares` in the source (they're the
all-target versions of the same heal, byte 3's target nibble is the only
difference in `TechniqueData`) — the port's "`_MULTI_ID`" naming for those
three is accurate, not a bug, and I want to flag that explicitly since it
was the one place I initially suspected a real mixup and it turned out not
to be.

## The fix (implemented)

`battle_party_source_technique()`'s fallthrough now does exactly what
`battle_party_source_skill()`'s own non-weapon fallback already does:

```c
int pose=battle_party_cast_pose_index(party[actor].id);
if(pose>=0){battle_party_pose_actor=(uint8_t)actor;battle_party_pose_index=(uint8_t)pose;}
battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);battle_action_hold(enemies,4u);
battle_execute_cmd(party,pn,enemies,actor,c,current_target);
battle_party_pose_actor=0xFFu;battle_party_pose_index=0xFFu;
battle_present_action(party,pn,enemies,*current_target,0u,actor,c,0u);
return 1;
```

I also added `actor>=pn` to the top guard clause, matching
`battle_party_source_skill()`'s own guard (the sole caller already
guarantees this, so it's a defensive no-op, not a behavior change).

**This is a presentation-only fix.** The effect/power/target/element/TP-cost
arithmetic is the exact same `battle_execute_cmd()` every technique already
went through — I did not touch numbers, targeting, or resource costs for
any of the 13 already-covered techniques or the 25 fallback ones.

**Why it's safe / doesn't double-apply the effect:** the caller,
`battle_resolve_mixed_round()`, does `if(!executed)battle_execute_cmd(...)`
right after the dispatch chain. Before this fix, an uncovered technique made
`battle_party_source_technique()` return 0, so `executed` stayed 0 and the
caller ran `battle_execute_cmd()` itself. Now the function always returns 1
(every technique either hits a dedicated handler or the new fallback, both
of which call `battle_execute_cmd()` exactly once internally), so the
caller's own call is correctly skipped every time — traced and confirmed in
`tests_pscstyle_v05_technique_presentation.py`.

### Verification

- New test `tests_pscstyle_v05_technique_presentation.py` (added to
  `scripts/validate_current.sh`): reads the real `TechniqueData` table
  straight out of `ps4built.bin` at its actual ROM offset, independently
  re-derives which of the 40 ids are battle-usable from the ROM bytes
  (matching the shipped `rom8(p+2u)&0x10u` filter bit-for-bit), and asserts
  it comes out to exactly the 38/2 split above; parses the dedicated-id set
  out of the real dispatcher source and confirms it's exactly the 13
  expected ids; extracts the whole `battle_party_source_technique()`
  function body and confirms it calls `battle_execute_cmd()` exactly once
  and always ends in `return 1;`; and confirms the caller's
  `if(!executed)battle_execute_cmd(...)` guard is still in place. All of
  this is re-derived from the ROM and the live source on every run, not
  hardcoded — it'll catch drift if either changes later.
- All 12 pre-existing gates in `validate_current.sh` still pass unmodified.
- Full Python asset/build pipeline still runs clean; host
  `gcc -fsyntax-only -std=c11 -Wall -Wextra` still reports zero new warnings
  for the changed function.
- **Not verified:** what the pose actually looks like on screen. I don't
  have an emulator here. `battle_party_cast_pose_index()` already has a
  pose entry for every one of the 11 playable characters (it's the same
  table `battle_party_source_skill()` already relies on), so this shouldn't
  be able to silently no-op, but please cast something like Zan or Deban in
  a real battle after building and confirm the character visibly raises
  into a casting stance instead of standing in their idle pose.

## What's still missing (not implemented, roadmap for next pass)

**20 distinct technique visual effects** (covering the 22 fallback ids not
already paired up above): Giwat, Nawat, Zan, Gizan, Nazan, Gra, Gigra,
Nagra, Megid, Brose, Vol, Savol, Gelun(+Doran), Seals, Rimit, Shift, Saner,
Deban, Feeve, Anti(+Rimpa), Rever(+Regen), Arows.

Two things I found while reading the source `TechObj_*` routines that should
save real time on this:

- **Shift, Saner, Deban, Feeve, Anti, and Rever are not five separate
  effects.** `TechObj_Deban`/`TechObj_Anti`/`TechObj_Rever` all branch
  straight into the same shared code (`loc_97BA`/`loc_978A` in `ps4.asm`)
  that `TechObj_Saner` uses, just with a different palette-object id
  parameter (`$48C`/`$490`/`$49C`/`$4A4`) — almost certainly the same
  glow/sparkle sprite recolored per spell. `TechObj_Feeve` literally just
  calls `TechObj_Saner` and tweaks one byte. This is very likely **one**
  generic "status effect glow" implementation with a color/duration
  parameter, not six bespoke ones. `Shift` is the outlier of this group —
  same *shape* of routine, but reads `Battle_Ability_Effects` differently
  (a single-target check vs. the others' up-to-5-target loop) — worth
  double-checking it can reuse the same template before assuming it can.
- **Zan/Gizan/Nazan and Gra/Gigra/Nagra each still need per-tier art**, but
  drive through the same generic `CharTech_LoadBattleObj` /
  `CharTech_LoadBattleObj_MultiTarget` / `CharTech_LoadBattleObjWithPos`
  primitives the source uses everywhere — the existing Foi/Wat/Tsu family
  code in this port (`battle_party_foi_source_technique()` etc.) is the
  right template to copy the *driving logic* from; only the pixel art and
  positions are new per tier. The Gra family additionally shares one common
  impact/burst sub-object (`$454`) across all three tiers.

I did not attempt any of this: each one needs its own pixel-art extraction
from the ROM (a `generate_technique_objects.py`-style tool addition per
effect), exact positioning/timing pulled from its `TechObj_*` routine, and —
critically — a way to look at the result, which I don't have here. Getting
one wrong could look worse than the current plain-but-honest cast pose.

**Skills:** only Vortex (id 6), Earth (id 31), and Vision (id 47) have a
bespoke effect in this port, out of 47 distinct `SkillObj_*` routines in the
source (54 ids total, 7 of them "2"-suffixed variants that already share
their base skill's object, e.g. `Eliminat2`→`SkillObj_Eliminat`). I did
**not** change anything here: every other skill already resolves through
`battle_party_source_skill()` to either the character's real weapon-swing
animation (`battle_party_source_attack()`, for the ~30 weapon-flagged
skills, byte 2 bit 7 set in `SkillData`) or a proper cast pose (for the
rest) before applying its effect — there's no silent no-presentation case
like techniques had, just plainer-than-original combat art. If you want the
same kind of roadmap doc for skills that I built for techniques above
(full id/name/shared-object table), that's a quick follow-up; I stopped
here since techniques were the actual reported problem.

**Enemy skills:** `battle_enemy_source_action()` only special-cases a
handful of named enemy attacks (Sweeper laser/lightning, Jurafa's flame
launch and micro-missile barrage); every other enemy special attack falls
back to the same generic name-banner + hold your `battle_present_action()`
already gives ordinary attacks. I looked at this only briefly — it did not
get the same disassembly deep-dive as player techniques/skills this pass,
since you specifically flagged player skills/techniques as the concern.
Flagging as an open question rather than a confirmed gap.

## Suggested next pass

1. Build + validate locally, then actually cast a fallback technique (Zan,
   Deban, whatever) in a real battle and confirm the cast pose shows up and
   looks right, and confirm damage/healing/status numbers are unchanged
   from before this patch for both fallback and already-covered techniques.
2. Pick the status-effect glow template (Shift/Saner/Deban/Feeve/Anti+Rimpa/
   Rever+Regen) as the first bespoke-effect target — it's most likely a
   single shared implementation covering 8 technique ids at once, the best
   ratio of effort to coverage of anything on the missing list.
3. Then the Zan and Gra elemental families, reusing the Foi/Wat/Tsu code as
   a driving-logic template.
4. Megid, Brose, Vol/Savol, Gelun/Doran, Seals, Rimit, Arows, Giwat/Nawat
   round out full technique coverage.
5. If skills still feel underwhelming after that, revisit with the same
   treatment — but by design nothing there is currently broken, only plain.
