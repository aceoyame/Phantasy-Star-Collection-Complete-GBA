# v0.4-v0.6 delta — how to apply

This is a small, targeted delta on top of your existing
`PS4-PSCSTYLE-v0.3-CLAUDE-DEEP-PASS-2026-09-23` bundle, covering three
rounds of work in one session. It is not a re-export of the whole project
(reference ROMs, disassembly, comparison ROMs and donor sources are all
unchanged and not repeated here).

**Read in this order:**
1. `current/PS4-PSCSTYLE-v0.6-COMBAT-FIX-HANDOFF-2026-09-24.md` — the melee
   "always does 1 damage" root-cause fix (start here, this is the big one),
   plus the RUN/Defend/item investigation and the field-bug investigation.
2. `current/PS4-PSCSTYLE-v0.5-BATTLE-ANIMATION-HANDOFF-2026-09-24.md` — the
   missing-technique-animation fix and full coverage table.
3. `current/PS4-PSCSTYLE-v0.4-BATTLE-SPEED-HANDOFF-2026-09-23.md` — the
   battle-death-flash performance fix.

## Files in this delta

- `current/source/gba/title_runtime_v06.c` — all three rounds' changes,
  applied in order; everything else byte-for-byte identical to your v0.3
  source. `current/title_runtime_v06.c.full-session.diff` is the complete,
  isolated unified diff against your original upload if you'd rather
  review or patch it in by hand.
- `current/source/tests_pscstyle_v04_battle_death_flash.py`,
  `tests_pscstyle_v05_technique_presentation.py`,
  `tests_pscstyle_v06_battle_fill_stats.py` — the three new regression/
  differential tests, one per round.
- `scripts/validate_current.sh` — updated to run all three.

## To apply

Copy the files under `current/` and `scripts/` over the matching paths in
your working copy (or `git apply current/title_runtime_v06.c.full-session.diff`
against your original `title_runtime_v06.c` if you're tracking this in
git), then:

```bash
./scripts/build_current.sh
./scripts/validate_current.sh
```

I ran both of these here as far as this sandbox allows (no ARM toolchain,
no emulator): all Python codegen and the full validation suite (12
pre-existing gates + 3 new ones) pass, and the file host-compiles clean
under `gcc -fsyntax-only -std=c11 -Wall -Wextra` with zero new warnings.
None of it has been built into a `.gba` or run in mGBA/hardware — please do
that before trusting it in play. Priority for your first real playtest:
start a fresh save and confirm melee damage and RUN both work properly
(the v0.6 fix); that one affects essentially the whole game.
