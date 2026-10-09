# PSIV PSCSTYLE v0.4 — battle-speed pass (Claude, 2026-09-23)

This is the direct follow-up to `START_HERE_CLAUDE.md` and
`current/PS4-PSCSTYLE-v0.3-SPEED-HANDOFF-2026-09-23.md`. It implements one
Priority-1 optimization end to end, and does deeper research (but no code
changes) on the rest of Priority 1 and all of Priority 2.

## Environment constraint that shaped this pass

This container has Python 3 and host `gcc`/`g++`, but **no ARM7/GBA
toolchain and no network access** (no `arm-none-eabi-gcc`, no `clang`+`lld`,
and `apt`/`pip` package fetches are blocked). It cannot produce a `.gba`
binary and cannot run mGBA or real hardware. Every conclusion below that
would normally come from a profiler or a listening test instead comes from
static analysis of the source plus:

- Re-running the project's own `tools/generate_*.py` asset pipeline against
  `references/roms/ps4built.bin` (hash-verified: matches
  `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`) to
  regenerate every derived header (`psc_field_predecode.h`,
  `psc_runtime_plan.h`, `psc_panel_bank.h`, `psc_field_fastassets.h`, etc.).
- Host-compiling the full, real `gba/title_runtime_v06.c` with
  `gcc -fsyntax-only -std=c11 -Wall -Wextra` against those regenerated
  headers. This is not the ARM7TDMI target and proves nothing about cycle
  timing, but it does prove the changed code parses, type-checks, and
  introduces **zero new warnings** under `-Wall -Wextra` against the
  otherwise-identical, warning-heavy baseline file.
- Running `./scripts/build_current.sh` end to end: every Python
  code-generation step passes, and the build only stops at the final
  `arm-none-eabi-gcc`/`clang` invocation, which is the expected, environment
  -only gap, not a defect introduced here.
- Running `./scripts/validate_current.sh` (all 11 existing gates) plus one
  new gate added by this pass. All pass.

**None of this is a substitute for your own build + mGBA/hardware pass.**
Please run `./scripts/build_current.sh` and `./scripts/validate_current.sh`
locally, and actually fight a battle to (at minimum) confirm a party death
still looks correct, before trusting this in play.

## Implemented: `battle_process_new_party_deaths` dirty-rectangle cache

This is the "particularly expensive obvious case" `START_HERE_CLAUDE.md`
calls out by name: the `$20`-frame (32-frame) Character_Dead flash was
calling `battle_compose_igg()` + `battle_present_page0()` on **every** tick
purely to toggle one character's visibility bit — rebuilding the full
240x160 background from ROM, redrawing every party member, every
event-formation enemy, the HUD and the damage overlays, then DMA'ing the
whole page and both full palette banks to VRAM, 32 times, to flash one
sprite.

### Why it's correct

Across this loop, `party`, `enemies`, `target` and every other
`battle_compose_igg()` input are frozen except one bit of
`battle_party_visible_mask`. That means the only pixels that can legally
differ between two "visible" frames — or between two "hidden" frames — are
inside the dying character's own fixed sprite rectangle, **unless enemy
idle animation also advanced that tick**: `battle_draw_enemy()` paints
enemies straight into `field_pause_fb` for every formation except event
index 0 (whose three slots are pure OBJ hardware sprites synced through
`battle_obj_sync()` and never touch the framebuffer at all).

The new code:

1. Composites each visibility state (visible / hidden) exactly once, and
   caches that state's `36x34` rectangle (`BATTLE_PARTY_W x
   BATTLE_PARTY_H`) in a small static buffer.
2. On every later frame that repeats an already-cached visibility state, it
   replaces the full recompose + full-page present with a byte-exact
   rectangle restore (`battle_death_flash_restore`) plus
   `battle_present_band()` over just those rows.
3. Uses `battle_enemy_anim_advance_generic()`'s return value — previously
   discarded with `(void)` — to invalidate **both** cached rectangles the
   instant enemy animation actually changes, so the next frame that needs
   either state is rebuilt through the exact original, unabridged path.
4. Falls back to the exact original path unconditionally if the slot's
   rectangle geometry ever fails the same bounds check
   `battle_blit_party_projected()` already uses (defensive; in practice
   always passes for slots 0–4).

Worst case (enemies animate on literally every tick) this degrades to
*exactly* the pre-v0.4 behavior — same number of full recomposes, same
pixels, same order of operations, same `ps4_audio_prepare_frame()` /
`waitvb()` call cadence. It is structurally impossible for it to do *less*
compositing work than correctness allows, only equal or less.

### Differential test

`current/source/tests_pscstyle_v04_battle_death_flash.py` (added to
`scripts/validate_current.sh`):

- Pins the exact new code (buffer declarations, cache-hit condition,
  invalidation line, narrow-band present, the retained original
  recompose+present call) as literal substrings of
  `gba/title_runtime_v06.c`, so this test breaks if the shipped algorithm's
  shape changes.
- Re-implements both the pre-v0.4 loop and the new loop as literal, ordered
  Python translations of the same state machine, then differentially tests
  them against the exact worst case (animation every tick), the exact best
  case (no animation), a single mid-sequence invalidation, and 3000
  randomized animation schedules. Every scenario is asserted pixel-for-pixel
  identical between old and new, and the new algorithm is asserted to never
  issue more full recomposes than the old one.

All 11 pre-existing gates in `validate_current.sh` still pass unmodified.

### Expected impact (byte-accounting, not a profiler)

Per full `battle_compose_igg()`+`battle_present_page0()` cycle:
`38,400` (background ROM→EWRAM) + `512` (palette load) + `38,400`
(fb→VRAM) + `512` + `512` (BG/OBJ palette) = **78,336 bytes of DMA
traffic**, on top of the CPU-side redraw work (background is already
counted; this excludes the per-pixel sprite/enemy/text drawing loops, which
`START_HERE_CLAUDE.md` explicitly flags as the larger, harder-to-quantify
cost).

- **Original:** 32 x 78,336 = 2,506,752 bytes of DMA traffic, and 32 full
  CPU redraws, *per dying character, per battle*.
- **New, worst case** (enemy animation changes every tick): identical to
  original — zero regression.
- **New, best case** (no enemy animation change during the flash — the only
  possible outcome for the OBJ-based event-0 formation, and a plausible
  outcome elsewhere whenever no alive enemy's animation timer elapses):
  2 full cycles + 30 narrow-band cycles. Each narrow-band cycle is one
  `240 x 34` row-band DMA (`8,160` bytes — `battle_present_band()` transfers
  full-width rows, not just the changed columns; the untouched columns in
  that band are simply re-written with their already-correct content) plus
  a `1,224`-byte CPU rectangle copy, with **zero** background/palette DMA
  and **zero** enemy/HUD/party-frame/damage-overlay redraw work.
  Total: `2 x 78,336 + 30 x 8,160 = 401,472` bytes — about an **84%
  reduction in DMA traffic** for the full death-flash sequence, and the CPU
  compositing work (the larger, unmeasured cost) drops from 32 full
  recomposites to 2.

### Memory cost

Two new `static uint8_t` buffers, `36 x 34 = 1,224` bytes each, `2,448`
bytes total, landing in the linker's ordinary `.bss` → EWRAM region (per
`gba/link.ld`, anything not explicitly tagged `.iwram.text*` goes to the
24 KiB EWRAM `.bss`/`.data` block, not the separately-placed
`field_pause_fb`/`field_pause_pal` scratch region at `0x0202A000`, and not
the 32 KiB IWRAM bank). Per the v0.3 handoff's own numbers that region was
at `13,176 / 24,576` bytes; this pass brings it to `15,624 / 24,576`,
leaving `8,952` bytes of headroom. IWRAM (`4,644` bytes free per the v0.3
handoff) is untouched by this change.

## Investigated but intentionally not implemented this pass

### `battle_present_action()` / `battle_party_source_attack()`'s per-pose-frame loop

This is structurally the *same anti-pattern* — a full
`battle_compose_igg()`+`battle_present_page0()` on every one of a physical
attack's animation-pose frames — and is likely **more frequent in real play**
than the death flash, since it fires on every party physical attack. I did
not implement a fix for it this pass; the death-flash technique does not
transfer directly, and a naive port would be unsafe:

- Unlike the death flash (which only alternates between **two** already-seen
  states), every pose frame here shows genuinely new sprite content, so
  there is no "repeat a cached state" opportunity. The right shape is
  instead: composite the scene **once** with the attacker's own rectangle
  left in its pre-attack (clean) state, cache *that* rectangle, then on each
  pose frame restore the clean rectangle and blit the new pose's pixels
  directly (`battle_party_attack_pixels()` + the existing
  `battle_blit_party_projected()`) before a narrow `battle_present_band()`.
- Between pose frames, `battle_action_hold()` already calls
  `battle_ui_redraw_generic_anim()` for event/random battles when enemy
  animation changes — and that function **already does its own dirty-band
  redraw that can overlap the attacker's rectangle**
  (`if(y1>64u&&y0<112u&&battle_ui_party) ... battle_draw_party_frame(...)`
  in `gba/title_runtime_v06.c`). A cached "clean rectangle" for the attacker
  would go stale exactly when that fires, and would need to be invalidated
  by the *same* `battle_enemy_anim_advance_generic()` change signal, wired
  through `battle_action_hold()`, not just checked locally.
- `battle_present_action()` is also called for non-looped cases (a single
  static cast pose in `battle_party_source_skill()`), so any cache lifetime
  has to be scoped to the specific calling loop, not baked into
  `battle_present_action()` itself, or a one-off call would read a stale
  cache left over from a previous attack sequence.

This is a real, well-understood, and probably larger win than the one
shipped here — it is next in line, with the design already sketched above.

### Palette-DMA elision in `battle_compose_igg()` / `battle_present_page0()`

`battle_compose_igg()` unconditionally re-fetches the battle palette from
ROM, and `battle_present_page0()` unconditionally DMAs both BG and OBJ
palette banks, on every single call throughout a battle, even though the
palette is usually unchanged between consecutive calls within one round.
I looked at caching "is `field_pause_pal` still current" but rejected it
for this pass: `field_pause_fb`/`field_pause_pal` are a **shared** scratch
buffer also used by non-battle "pause"-style screens elsewhere in
`gba/title_runtime_v06.c` (see `pause_capture_palette()`,
`panel_present_scaled()`, etc.), and proving no other code path can
mutate them between two `battle_compose_igg()` calls would require
auditing every caller across the whole 7,400-line file — exactly the kind
of "large architectural rewrite" `START_HERE_CLAUDE.md` says to measure
before attempting, and I have no profiler here to measure with. Flagging it
as a real, bounded-looking opportunity for whoever has hardware/emulator
access to confirm the buffer-lifetime assumption.

### Priority 2 — audio stutter/crackle

I read `ps4_audio_stream.c` in full and worked through the exact cycle
math (`528 x 532 = 280,896`, one video frame) in
`START_HERE_CLAUDE.md`'s favor. I deliberately made **no changes** here:

- The handoff's own instructions are explicit that any change to the
  Timer0/FIFO commit cadence must not ship "without emulator/hardware
  listening tests because FIFO fill level matters," and that instrumentation
  should come *before* rewriting. I have neither an emulator nor hardware in
  this environment, so I cannot meet that bar.
- I considered adding just the suggested counters (`REG_VCOUNT` on entry to
  `waitvb()`, whether `ps4_audio_prepare_frame()` had already run) but
  rejected it too: capturing them requires touching `waitvb()` itself, which
  runs on literally every video frame across the entire game (field, menus,
  cutscenes, battle) — a far larger blast radius than anything else in this
  pass, and I have no way to confirm the extra branch/read doesn't perturb
  the exact cycle timing the audio design depends on. Given
  `battle_process_new_party_deaths` above already cuts a large, real chunk
  of heavy-frame CPU work out of one of the two battle-heavy code paths
  (and shortens the *other* one is the next recommended step), it's worth
  re-checking whether crackle correlates with battle load again after both
  battle-side fixes land, before instrumenting the audio path itself.

If you do want the instrumentation next: add two `uint32_t` counters in
`ps4_audio_stream.c` (`total_frames`, `already_ready_at_waitvb`), expose
them via a small accessor, and read `prepared_ready`'s value at the very
top of `waitvb()` *before* its own `ps4_audio_prepare_frame()` call — that
tells you how often the "prepare early" pattern (the explicit early call at
the top of `battle_compose_igg()`) is actually landing before `waitvb()`
versus `waitvb()` doing the prep itself at the last possible moment.

## Known branch-fusion bugs

Not investigated this pass, per `START_HERE_CLAUDE.md`'s explicit
instruction to defer correctness cleanup unless it blocks a performance
change. Nothing in this pass touches or depends on any of them.

## Suggested next pass

1. Build + validate locally (`./scripts/build_current.sh`,
   `./scripts/validate_current.sh`), then fight a battle and let a party
   member die to confirm the flash still looks right, and check the ROM's
   new `.bss` figure roughly matches the `15,624 / 24,576` estimate above.
2. If you have mGBA with a Lua/scripting console or a cycle counter,
   profiling one death flash and one physical-attack animation
   before/after would turn the byte-accounting above into real numbers —
   genuinely useful to hand back for a future pass.
3. `battle_present_action()` / attack-pose loop, per the design sketched
   above — likely the single highest-value remaining item.
4. Re-evaluate audio crackle after (1)-(3); instrument before touching
   `waitvb()`'s cadence.
