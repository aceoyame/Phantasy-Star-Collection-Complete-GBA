# Stage16 — Academy overworld OBJ VRAM reuse (new evidenced blocker)

**Date:** 2026-11-23. This is an offline forensic audit of real Stage14 emulator captures, **not a fresh emulator run or new ROM patch**.

Re-read the actual `stage14_overworld_snapshots.json` from the private Stage14 QA kit, checked the 7 field checkpoints of both Stage12 and Stage14, and compared complete step/frame/map/player/screenshot/sprite signatures. All 7 parent/candidate checkpoint signatures match.

**New finding:** GBA OBJ tile indices are **mutable VRAM slots**, not immutable character/pose identifiers. With palette bank 2 fixed, tile index 448 held **5 distinct 4bpp sprite SHA-256 values**, index 480 held **4**, and index 488 held **2** during the seven checkpoints. This was reproduced independently in both Stage12 and Stage14 records. Therefore a static mapping `tile_index → Genesis pose` is invalid and would silently replace the wrong animation frames.

A source-only repeatable auditor is committed as [audit_dynamic_obj_slots.py](audit_dynamic_obj_slots.py). Its input is the private Stage14 snapshot JSON; no commercial assets are embedded or published.

**Next executable step:** instrument the live mGBA GBA OBJ VRAM write path or DMA copy path, logging `frame, destination OBJ VRAM address, ROM source address, byte count, map, actor, facing/animation phase`. In a synchronized Genesis scene, trace original Genesis sprite source/pointer and actor pose for each observed phase. Only then patch source-indexed art, never a VRAM destination index.

**Limitations:** The accepted R235 SHA `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308` could not be independently staged from Library in this run; R86/R203/R209/R234 original archives also were not found by Library title search. Stage14 local ROM independently SHA-verified as `c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609` (32 MiB). No new playable build, cross-core gameplay test, FIN playthrough or Micro hardware acceptance is claimed. Preserve original scenic backgrounds, portrait border and previous Stage14 art. Production R235/R236 and main untouched.
