# R235 Stage 10 — original-style menu border source audit

Original source: `r197/build_r197.py` in the user's R203 complete archive. Actual accepted R235 `PSC_R235_FIXB_NEWGAME_RELEASE.gba` and the Stage10 experimental trial were compared byte-for-byte.

- R235 menu corner/horizontal border tiles at ROM offset `0x8890A9+32..95` **exactly match** the R197 double-ridge source data designed to reproduce the Genesis menu's twin rails.
- R235's battle-HUD 144-byte window stamp block at offset `0xE5FAB0` **exactly matches** the R197 6×6 conversion from those same border tiles.
- The HUD text vertical add-immediate values remain `14` and `22` pixels in R235, at ARM offsets `0x805FD0`, `0x805FEC`, `0x806004`, `0x806020` and Thumb `0x83D42C`, `0x83D440`, `0x83D44E`, `0x83D45E`.
- Stage10 changes **none** of these UI tile/stamp/code bytes, or the 8,192-byte dialogue OBJ template. Source comparison and direct ROM binary comparison both performed.
- Stage10 has a 27-checkpoint 3-core title/field/UI regression PASS and 12 real battle checkpoint comparisons with all visual changes confined to party character positions.

**Result:** The GBA already has the inherited Genesis-style double-ridge borders; blanket replacement would risk a regression. Changes to text, typeface, cursor and portrait highlight should be grounded in matched original Genesis and R235 live screenshots, with border geometry and text positions validated together. **No new UI graphics patch is claimed in Stage10.**