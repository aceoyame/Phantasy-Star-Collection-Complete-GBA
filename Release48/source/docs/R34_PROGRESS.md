# R34 recovery status

Baseline: final R33, unchanged and retained separately. Final R34 build and focused checks complete.

Authorized goal: native source-derived Psyco Wand ($0A) and Zio Defeated ($0B) continuations. Preserve accepted audio and first 8 MiB shell. No flag bypasses.

Source corrections confirmed before implementation: AfterAlysDeath=$63, AfterAlysDeath2=$67, GryzGone=$68, MotaSpaceport=$66, PlateEngine=$61, BioPlant_B4_Part2=$AC, HerLastBreath=$A4, Tandle=$CE, RidingElevator=$E0. Source dialogue continuations must survive real map reloads.

Initial checkpoint status: not built. Final R34 is built and checked; playthrough remains unverified.

Checkpoint 02: both native scene handlers and five reload continuations implemented. Hahn now uses literal type $0C, original source mappings and raw character art; actor bank prefix retained. Dialogue restoration uses parsed F7 seams, not guessed text IDs. Zio's bit-3 same-map reload preserves 320 bytes of party carriers on the stack. Source bugfixes=0 confirmed from ROM; no optional Chaz revival injected. Focused scene probes and ARM build pending.

Checkpoint 03: ARM build and sanitizer scene checks pass. Original ROM Tree6 $2E has six F7 seams; Tree36 $00 has five. Exact segment offsets are tested through all five continuations. Focused R31, R33 chest/Alys, R32 vehicle/travel/maps and R30 controller regressions pass. Byte comparisons pass for 27 accepted audio files, 8 MiB shell and complete R33 pose-bank prefix. Module 24,316,288 bytes; IWRAM end 03006C88; EWRAM end 02029FF9. Normal-save/emulator/hardware verification remains unperformed. Final notes and actor preview still being prepared.

Checkpoint 04 is the final cumulative handoff, including R34_NOTES.md, the original-source audit, focused logs and the inspected Hahn preview. Final ROM is provided separately. Resume next with MeetingWren, FallingSatellite and CrashLanding; keep source and accepted audio preservation constraints.

The complete final ROM is durably saved inside PSIV_R34_ROM_2026-09-30.zip; extract RUN_THIS_R34_PSC_STORY_CONTINUATIONS.gba. A standalone raw-ROM save exceeded the storage limit, so use this verified archive. No earlier files were removed.
