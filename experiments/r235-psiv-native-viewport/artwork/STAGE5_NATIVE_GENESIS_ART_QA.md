# Stage 5 — original Genesis artwork trial (manga + dialogue portraits)

**Status:** experimental, not a release candidate. This branch has no copyrighted ROM, Genesis extracted images, sound or game binaries.

## Source provenance
- Parent accepted R235: SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308` (32 MiB).
- Original Genesis US PSIV ROM: SHA256 `511f35cc11f88316f8b8940e28ab298bd75a4da193672a80172884d6eb913b6a`; recovered from the user's privately held `PSC_R175_CUMULATIVE_HANDOFF.zip/reference/US_PSIV_fe236442.bin`.
- Decoder/conversion sources from privately held R203 `r196/build_r196.py`, `r196/md_compression.py`, `r196/projection.py`, and R234 `source/src/portraits.py`.
- Verified Stage 4 field camera ROM: SHA256 `f3a80bad91b6d3f1809bf1e09ea09f887e204428aeb55247bf67426caf03f89d`.

## Implemented in separate test binary
- Manga panel IDs **00–09**: generated from original Genesis 320×224 source without resampling, fixed common 240×160 source crop starting x=48, y=0; re-encoded using the original R234 compressed-span GBA decoder. They use the existing panel palette and id table. The chosen common crop preserves about 97.17% of the colored source pixels over this 10-panel subset, but **some compositions still clip at the screen edge**. This is *not* a full story-panel conversion.
- Dialogue portrait IDs **01–39**: the exact 48×48 Genesis art was cropped 1:1 to 40×40; only the inner 36×36 was replaced, keeping R235's original 2-pixel border and 40×40/25-tile storage, palette and positioning. 30,184 portrait pixels were modified. In-game expression and placement not yet validated.
- Safely merged with Stage 4 native field camera. No write overlap. Audited R234 reclaimed region `0x0922B000–0x0923446F` holds new manga records; only ten panel table entries and the pre-existing 39 portrait records are additionally changed. GBA header preserved.

**Combined experimental ROM:** SHA256 `c95c2830de0cfe6bb271edfbfe6b8ea26f32a960dffd3e9818c390713529c954`, CRC32 `33d6f4e9`, exactly 33,554,432 bytes.

## Verified testing
1. All ten modified indexed panels were executed through **R235's actual ARM manga decoder in mGBA**. All 38,400 framebuffer bytes per invocation exactly matched the intended source-native crop, each time.
2. At **nine title/PSIV launch/field checkpoints per core**, mGBA, VBA-M and the Delta-style VBA-M core all produced pixel-identical images to the Stage 4 baseline, with equal maps, player positions, and frames (27 checkpoints). The specific art was not exercised by these startup checks.
3. The unified rebuild from the three user source archives independently reproduced the exact combined SHA256 and CRC32.
4. An exact side-by-side preview was rendered from actual R235 and modified ROM tile/stream data (not generated artwork).

## Critical limits
- Live *story presentation* of panels and portraits (overlap/timing/expressions/window borders) remains untested. Decoder correctness does not establish scene correctness.
- Only 10/166 main precompiled panels have changed. Three extension records and other story assets remain unchanged.
- The earlier Stage4 camera transition settlement issue remains open.
- No full GBA story playthrough, FIN verification, or hardware/Delta-iOS test has occurred for the edited art.
- Do not promote this build until screenshots from live cutscene scenes are matched to Genesis without cropping important characters or text.

## Continuation
Next: trigger actual manga story events (Alshline, Alys Wounded, Saving Demi, etc.) and compare frame sequences on each emulator core. Trace portrait sprite tile publication and palettes during live dialogue. Fix border cropping/scene composition first. Then extend the original-art approach to the remaining main and extended manga panels, portrait poses, battle backgrounds, effects, enemies, and remaining resampled/cutscene assets. Keep field renderer native as it already largely is. Preserve R235/rollback and the normal 32 MiB GBA/Micro timing requirements.

All build scripts, output hashes, small reference images, and QA manifests are packaged in the private Stage5 source/QA kit. Do not commit any commercial game assets to this public repository.