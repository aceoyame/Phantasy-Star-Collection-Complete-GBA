# Verified source lineage available in the user's ChatGPT Library

Recorded 2026-11-22 (US Eastern). **Metadata only; no copyrighted ROM bytes or extracted artwork have been added to GitHub.**

## Located archives
- `/PSC_R86_CUMULATIVE_HANDOFF_20261103.zip` (404,087,718 bytes, 6,881 archive members): historical field / actor / camera source in `source/field68.c`, `source/publish72.c`, `source/depth61.c`, `source/obj81.s`, `source/field74.c`, plus `rebuild_r86.py`. **Requires exact R85 parent ROM**, so not a standalone from-scratch source build.
- `/PSC_R203_COMPLETE.zip` (334,156,733 bytes, 9,931 archive members): versioned R191–R203 build scripts, native battle renderer at `r194/src/battle_render.c`, UI and layout sources, patch files, tests, reference images and ROM revisions. **R203 build script requires exact SHA-verified R202 parent ROM.**
- `/PSC_R209_COMPLETE.zip` (30,295,249 bytes, 852 members): `r209/build_r209.py`, `r209/src/enemy_effects.c`, compiled ROM, tests, and an R208-to-R209 BPS patch.
- `/Phantasy-Star-Collection-Complete-GBA/PSC_R234_SPACE_AUDIO_FIX_HANDOFF.zip` (19,030,154 bytes): 32 MiB R234 ROM; `source/build_r234.py`, portrait/audio and other incremental source, patches; requires three earlier ROMs for a source rebuild.

## Exact accepted R235 ancestry confirmed by **actual ROM byte comparison**
- R234 (from the archived handoff) SHA256: `88ea42999aff3c3929d4b9de093272e0eaf7546efe5cd5fc1908e5ec30f7030a`.
- R235 (Library `/PSC_R235_FIXB_NEWGAME_RELEASE.gba`) SHA256: `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.
- Both exactly 33,554,432 bytes, with **23 differing bytes** in three runs: ROM offsets `0x801A60-0x801A63`, `0x122AA10-0x122AA17`, and `0x122AA19-0x122AA23`. Treat these as binary provenance, not equivalent to a complete source checkout.

## Engineering consequence
The Library **does** contain substantive PSIV rendering sources, revision-specific build scripts, verified late ROM parents, and incremental patches. The previous 'no source' conclusion was incorrect. However, the complete source for an R235 from-scratch rebuild has **not** been demonstrated. The test branch can use R86 field code and R194 battle code as historical references, then reverse trace/check runtime addresses against the **exact accepted R235 binary** before integrating live camera/menus. Do not assume older source maps directly to R235 current addresses without this verification.

This inventory is read-only and makes no changes to accepted ROMs.
