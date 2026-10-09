# R34 — Psyco Wand and Zio aftermath continuations

R34 continues final R33 toward normal gameplay. It adds native C paths for
Cutscene_PsycoWand ($0A) and Cutscene_ZioDefeated ($0B). The build and focused
host checks pass. This is a development build: normal-save gameplay, emulator
or hardware timing, and full-game completion remain unverified.

## Changes

- **Psyco Wand / Alys funeral:** opening dialogue, real Krup Inn reload,
  party destinations and facings, HerLastBreath music, original Tree6 $2E
  conversation, the complete explicitly authored panel montage and waits,
  recovery, real Krup exterior reload, foreground/background funeral layout
  changes, Saya and Hahn departure, Chaz's movement, camera return, and
  AfterAlysDeath ($63) / AfterAlysDeath2 ($67). Tree5 is restored at completion.
- **Zio / Nurvus aftermath:** Tandle cues, original Tree36 $00 dialogue,
  source white-tone cadence, Demi placement, same-map RefreshMap with bit 3,
  Demi's 16.16 drift, both original foreground elevator tables, 8/6-update
  table timing, power-down palette changes, panel $70, Motavia reload,
  existing native spaceport-appearance event, panels $73-$75, Gryz/Demi
  removal, final Motavia reload and GryzGone ($68), MotaSpaceport ($66),
  PlateEngine ($61). Party objects are hidden/restored around the spaceport.
- **Reload continuity:** five phases use the existing continuation bytes.
  A same-map bit-3 reload preserves all 320 bytes of source party objects
  using stack storage. Other R34 reloads use the authored map, previous-map,
  start coordinates, facing and alignment. Pending text is suspended before
  the outer dispatcher can consume it in the wrong phase.
- **Dialogue continuity:** reloads restore the exact original F7 segment.
  The cursor parser skips control operands. Tree6 $2E has six seams; Tree36
  $00 has five. Zio's post-spaceport text continues segment 5 rather than
  restarting entry 0.
- **Hahn field actor:** literal source type $0C, original directional mapping
  sequences and raw character art. Added 16 immutable poses / 4,096 bytes.
  The complete 68,800-byte R33 actor bank is retained as an identical prefix.
  New total: 303 poses / 72,896 bytes. The sprite preview was inspected.

Accepted source ROM bytes omit the disassembly's conditional Chaz revival
patch (`bugfixes=0`). R34 preserves that binary behavior. No story flag bypass,
audio retuning, frame skipping, cycle scaling, or new EWRAM global was added.

## Checks and limits

ASan/UBSan host checks compile the new scene handlers and native actors. They
verify actual-ROM dialogue offsets through every phase, panel order and wait
counts, Hahn motion and OBJ bounds, preservation through a destructive map
load stub, Demi's fixed-point drift without destination rewrites, both original
elevator byte tables and foreground-only writes, 8/6-update cadence, party and
64-byte carrier compaction, macros, outgoing equipment, and final flags/maps.

Movement orchestration, palette/display presentation and the spaceport effect
are stubbed where documented. These checks validate state and controller
behavior, not a normal-save playthrough. The existing spaceport handler retains
the port's earlier transient-object approximation; this turn does not certify
mechanical-object or Genesis visual parity. Native panel scaling, fades and
camera projection retain the port's presentation behavior.

R31 actor/story, R33 chest/Alys, R32 vehicle/ice/page, R32 travel/map-update and
R30 status/tile-animation regression probes pass. ARM7TDMI compilation and
linking succeed without compiler warnings. Focused byte comparisons confirm
all 27 accepted audio files, the first 8 MiB PSC shell and the R33 actor-bank
prefix are unchanged. No asset hashes or blanket asset regeneration were used.

| Build boundary | R34 |
|---|---:|
| Final GBA ROM | 33,554,432 bytes |
| PSIV module | 24,316,288 bytes |
| Remaining 24 MiB module slot | 849,536 bytes |
| IWRAM end | 0x03006C88 |
| Space before IRQ reserve | 120 bytes |
| EWRAM BSS end | 0x02029FF9 |
| Space before presentation arena | 7 bytes |
| Preserved-party loader stack frame | 344 bytes |

## Remaining work

Static dispatch coverage is now 156/161 ordinary events, **11/33 cutscenes**,
54/54 map-used updates, 98/98 conditions and 127/127 managers. A dispatch count
indicates a native path, not full behavioral parity or finishability.

Next story blockers are MeetingWren ($0C), FallingSatellite ($0D) and
CrashLanding ($0E). Five ordinary events remain: MonsenEarthquake ($2C),
EclipseTorchStolen ($46), EclipseTorchUsed ($47), MeetingLeRoof ($48) and
DyingBoy ($86). Other later scenes, mechanical field objects, vehicle battle
weapons, movement-heavy battle controllers, Profound Darkness and the ending
remain. See build_artifacts/R34_SUBSYSTEM_AUDIT.md for the full dispatch audit.

The R33 source-stride fragment anomaly is unchanged. Normal-save validation
should cover the Rune/Dorin route, Alys/Saya, Psyco Wand chest, funeral, Zio
battle and Nurvus/spaceport aftermath without forcing story flags.

## Checkpoints and recovery

Four cumulative R34 increments were saved as work progressed:

| Checkpoint | State |
|---|---|
| PSIV_R34_CHECKPOINT_01_INCREMENT_2026-09-30.zip | Workspace and source-confirmed continuation design |
| PSIV_R34_CHECKPOINT_02_INCREMENT_2026-09-30.zip | Both scene implementations and reload wiring, WIP |
| PSIV_R34_CHECKPOINT_03_INCREMENT_2026-09-30.zip | Scene probes, ARM build, regressions and preservation verified |
| PSIV_R34_CHECKPOINT_04_INCREMENT_2026-09-30.zip | Final cumulative source, notes, evidence and sprite preview |

Restore R32 using PSIV_R32_CHECKPOINT_01_2026-09-30.zip plus
PSIV_R32_CHECKPOINT_03_INCREMENT_2026-09-30.zip. Copy that tree to R33 and
overlay PSIV_R33_CHECKPOINT_04_INCREMENT_2026-09-30.zip. Copy R33 to R34 and
overlay R34 checkpoint 04. Earlier R34 increments are not required. The final
ROM is saved in PSIV_R34_ROM_2026-09-30.zip. Extract
RUN_THIS_R34_PSC_STORY_CONTINUATIONS.gba from that archive to run it. The ZIP
contains the complete ROM and does not require applying a binary patch.

Build from R34/source with `python3 build_r34.py` using Clang, LLD and
llvm-objcopy. This session's LLVM 18 binaries are in toolchain_download/bin;
libraries are in toolchain_download/root/usr/lib/x86_64-linux-gnu. Set PATH
and LD_LIBRARY_PATH to those directories. Only the small field pose bank is
regenerated by this wrapper; other accepted offline assets remain retained.

Run check_r31_story.py before check_r34_scenes.py, check_r33_story.py,
check_r33_alys.py and check_r32_vehicles.py; those reuse its generated host
harness. check_r32_travel_maps.py and check_r30_controllers.py are independent.
check_r34_preservation.py takes `--baseline <R33/source>`.
