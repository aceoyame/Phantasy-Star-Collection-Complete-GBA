# R33 — Alys story continuation and Psyco Wand chest

R33 continues the accepted R32 native C GBA PSC build. It adds the Alys-wounded
story scene and the Psyco Wand chest event. This is a further development build;
normal-save gameplay, hardware timing and full-game completion are unverified.

## Changes

- **Cutscene_AlysWounded ($09):** original opening dialogue, teleport/panel cue,
  optional Saya visit in Krup, healing stars, inn reload and the complete Tree6
  conversation/facing sequence. Hahn and Alys leave through source party and
  64-byte object-state compaction. Their macro entries are removed, equipment
  remains intact, Demi is inserted with ATTACK macro entries, and DemiJoined
  is set at the end. Both real map reloads resume through the existing phase
  state. Removing a lead character updates the native lead identity.
- **Event_PsycoWandChest ($2F):** Rune's approach, original visibility-dependent
  countdown, barrier break, 18 fragment objects, second dialogue, camera move,
  Gy-Laguiah reveal, source animation-completion gates and event battle $05
  handoff. The GyLaguiah story flag is set after the reveal and dialogue.
- **Field actors:** source barriers, fragments, guardian, Saya movement, healing
  stars and Alys in bed. Added 223 immutable native OBJ poses. The first 64
  R32 poses and their 26,272-byte bank are retained byte for byte. New total:
  287 poses / 68,800 bytes. The generated preview was visually inspected.
- Fixed-frame source animation wrap now publishes render-flags bit 2, needed
  for the guardian's chained appearance/breath/idle states. The existing Saya
  event also uses the native healing-stars controller.

No new EWRAM globals, audio retuning, frame skipping, cycle scaling or story
flag bypasses were introduced. The original first 8 MiB PSC shell is retained.

## Validation and limits

Sanitized host probes pass for both Alys scene branches, party/carrier/macro
compaction, outgoing equipment, lead identity, both reload requests, Demi join,
the original three and ten Tree6 F7 seams, dialogue-tree restoration, healing
stars lifetime, Saya movement and stationary poses, and native OBJ bounds.
Chest probes pass flicker boundaries, the 11-update barrier break, 39-update
fragment lifetime and fixed-point velocities, the 244-update guardian reveal,
dialogue/camera sequencing and battle handoff.

These probes stub movement orchestration, text/display, fades or battles where
indicated. They establish native state/controller behavior, not a playthrough.
The R31 actor/story, R32 vehicle/ice/page, R32 travel/map-update, and R30 status/
tile-animation regression probes also pass. The ARM7TDMI build succeeds.

Focused byte comparisons confirm all 27 accepted audio files, the shell, and
the R32 actor-bank prefix are unchanged. No asset hashes were computed.

| Build boundary | R33 |
|---|---:|
| Final GBA ROM | 33,554,432 bytes |
| PSIV module | 24,306,016 bytes |
| Remaining 24 MiB slot space | 859,808 bytes |
| IWRAM end | 0x03006C88 |
| Space before IRQ reserve | 120 bytes |
| EWRAM BSS end | 0x02029FF9 |
| Space before presentation arena | 7 bytes |

The accepted ROM's fragment helper reads eight bytes, then executes
`LEA 8(A0),A0`: its literal stride is 16 bytes. With 18 objects this reads
beyond the 144-byte coordinate table into the following source code. R33
preserves that binary behavior, including offscreen fragments; it does not
silently substitute an intended eight-byte stride. This source anomaly and
the full scene's visual timing should be assessed during in-game verification.

Native panel scaling, fades, camera projection and general field orchestration
retain the port's presentation behavior; Genesis display/timing parity has not
been certified. The new code has no emulator or hardware verification yet.

## Coverage and next work

Static dispatch coverage is 156/161 ordinary events, 9/33 real cutscenes,
54/54 map-used updates, 98/98 map-used conditions and 127/127 map-used managers.
A dispatch count identifies a native path, not complete behavioral parity.

Next blockers on this story route are **Cutscene_PsycoWand ($0A)** and
**Cutscene_ZioDefeated ($0B)**. Remaining ordinary events are MonsenEarthquake
($2C), EclipseTorchStolen ($46), EclipseTorchUsed ($47), MeetingLeRoof ($48)
and DyingBoy ($86). Later cutscenes, mechanical field objects, vehicle battle
weapons, movement-heavy battle controllers, Profound Darkness phases and the
ending remain. The full audit is in build_artifacts/R33_SUBSYSTEM_AUDIT.md.

## Recovery checkpoints

Four cumulative R33 increments were saved as work progressed:

| Checkpoint | State |
|---|---|
| PSIV_R33_CHECKPOINT_01_INCREMENT_2026-09-30.zip | Chest actor implementation, marked WIP |
| PSIV_R33_CHECKPOINT_02_INCREMENT_2026-09-30.zip | Chest path and first verified ARM build |
| PSIV_R33_CHECKPOINT_03_INCREMENT_2026-09-30.zip | Alys scene continuations and focused probes |
| PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip | Final R33 source, notes, evidence and sprite preview |

Every increment is cumulative against final R32. Restore R32 using
PSIV_R32_CHECKPOINT_01_2026-09-30.zip plus
PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip. Copy R32 to R33, then overlay
checkpoint 04. Earlier R33 increments are not required. No checkpoint deletes
the accepted audio or baseline source. The ROM is also saved separately as
RUN_THIS_R33_PSC_STORY_PROGRESS.gba.

Build from R33/source with `python3 build_r33.py` using Clang/LLD/llvm-objcopy.
For this session, LLVM 18 lives under toolchain_download/bin, with libraries
under toolchain_download/root/usr/lib/x86_64-linux-gnu. Set PATH and
LD_LIBRARY_PATH accordingly. The build skips blanket asset regeneration.

To rerun the focused checks, run check_r31_story.py before check_r33_story.py,
check_r33_alys.py and check_r32_vehicles.py; the latter reuse its generated host
harness. check_r32_travel_maps.py and check_r30_controllers.py are independent.
check_r33_preservation.py takes `--baseline <R32/source>`.
