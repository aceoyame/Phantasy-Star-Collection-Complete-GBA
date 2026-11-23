# Stage 18 — Academy BG/OBJ transition: verified first-producer timing and diagnostic

Date: 2026-11-23. **Isolated experimental R235 viewport branch only. No ROM patched or published.**

## Independently verified private inputs

- Accepted production R235: 33,554,432 bytes, SHA-256 4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308.
- Stage14 experimental ROM: 33,554,432 bytes, SHA-256 c1ed9db09505fe2d17322efa9f7c4915e7a4cd57bdf2a3dc6f24d0f8f1d15609.
- Genuine Academy after-principal map18 savestate: 397,312 bytes, SHA-256 3a18bcce1f725baaeac2114f20edda5aca4aa4e482464aba6f15ee21b0454ae3.
- Native mGBA core and host probe from the owner's Library headless toolkit. Normal frame-indexed controller replay; no emulated timing changes.

## New executed machine-code breakpoint chronology

Map 18 changes to map 17 at frame 16555. Both ROMs have the identical first breakpoint hits from the same real gameplay state:

| Code execution | Accepted R235 first hit | Stage14 first hit | Register evidence |
|---|---:|---:|---|
| BG raster DMA0 setup at 0x08C7D17C | frame **16571** | frame **16571** | r0 = 0x04000010; r2 = 0x02034A20 |
| Actor-camera hook at 0x08CD3854 | frame **16572** | frame **16572** | r0 = 0x148; r1 = 0x18 |

This confirms the background presentation event precedes the actor-camera event by one emulated frame. It is a first-hit trace, not an instruction-complete count of all writes.

The Stage14 BG raster-scroll buffers already contain horizontal +16 / vertical +8 offsets relative to accepted R235. At frame16571 the three active OAM sprite entries are *unshifted and byte/position-identical* in the two cartridges: slot0 (40,102) / tile496; slots2 and 40 (152,86) / tiles480 and 448. At frame16573 Stage14 positions are exactly (24,94) and (136,78): the intended 16x8 shift has finally been published. Original native sprites/tile pixels/palettes remain unchanged.

## Controlled causal diagnostic — **HOST-ONLY OAM memory intervention**

Ran the same actual Academy route with accepted R235, untouched Stage14 and a *disposable Stage14 emulator instance*. Only the disposable instance had three source-verified OAM RAM entries temporarily changed by horizontal -16 / vertical -8 directly after frame16571. Exact SHA-256 checks, map17/player (328,24), slot identifiers, tile indices and shape checks prevent the diagnostic outside the captured scene. This is not a patch to the ROM or a game feature.

Exactly registered unscaled 224x152 RGB overlapping view:

| Frame | Untouched Stage14 RGB mismatch pixels | Test-only OAM-corrected mismatch pixels |
|---|---:|---:|
| 16570 | 0 | 0 |
| 16571 | 3,135 | 3,135 (test change applied *after* this frame rendered) |
| **16572** | **681** | **0 — exact pixel parity** |
| 16573–16578 | 0 on each | 0 on each |

**Conclusion:** The remaining 681-pixel mismatch at frame16572 is causally explained by OAM position publication latency. The normal actor-camera updater takes over by frame16573 without double-shifting the six observed following frames. This RAM oracle does **not** resolve the partially revealed background/misalignment at frame16571, nor prove a safe ROM fix.

Local (private) evidence:
- Full controlled replay JSON SHA256: ea55472f724c8c3ef959f3ccaa29b2857cead86149c8e2367a53e981debbc554.
- Live BG/OAM IO register ledger JSON SHA256: 50eaed1b75c4ea230cac185148a17893ed645226922acbeedfef5c91c737b5a0.

**New reusable probe source:** [diagnose_stage18_camera_publication.py](diagnose_stage18_camera_publication.py), SHA-pinned, read-only by default with an explicit optional RAM-only experimental oracle. It records actual BG/actor first hits, RGB overlap and map/player progression; no game art/ROM data embedded.

## Remaining engineering gate

Identify the **original first OAM publication routine** during the map17 scene reveal, before the normal actor-camera hook. Then implement a *source-guarded* opt-in synchronization of BG raster and sprite positioning, rather than blindly editing every OAM slot or introducing a new fast-EWRAM hack. Re-test transitions to other maps, dialogue/menus, battle views, source-matched Genesis geometry, mGBA plus VBA-M/Delta, cartridge size/checksum/save states and GBA/Micro timing before any candidate acceptance.

**No new playable cartridge in Stage18.** The existing Stage14 ROM is unchanged. Accepted R235, production R236 and GitHub main remain untouched; full FIN, cross-core transition acceptance and GBA Micro are unverified.