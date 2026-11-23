# Stage11 follow-on: authentic live overworld sprite inventory (read-only)

The field scene is **real accepted R235 Academy map #18** from the user's 2026-11-21 after-principal settled savestate. Imported into native mGBA from the user's own overnight QA archive. Compared Stage10 and Stage11 ROMs after 25 frames, without patching actor code.

- Stage10 SHA256 `d08d6c88b5c6f6dee2ae15925864e5c98d58b113859d063476ac86ea97161d56`
- Stage11 SHA256 `820892b5fa2c145a249876baba90144152d7ff1520fb865e43d2103bc2bb80b2`
- Both screenshots CRC32 **`6f6e85fe`**; pixel-exact comparison passes.
- GBA hardware display uses 1D OBJ tile mapping (DISPCNT bit 6) and 4bpp hardware sprites. Source geometry is read directly from 1,024-byte OAM, 4bpp OBJ VRAM and OBJ palette.
- Actual visible slot 0: X136, Y94, **16×32 px**, GBA OBJ tile 448, palette bank 2, 163 colored pixels, decoded tile SHA256 `342aff326d6c54d4b065331650954ef8ca3b55d1b842171fef33d75b89cf48d1`.
- Actual visible slot 1: X136, Y78, **16×32 px**, GBA OBJ tile 480, palette bank 2, 165 colored pixels, tile SHA256 `988e6ce52bae5adf1e817f60b8a7984600158c1739bfb83ce743d77a081bfdcf`.
- Both slots have **identical sprite metadata and bytes in Stage10 and Stage11**. Offscreen OAM slots at y160 were explicitly filtered and not counted as visible.
- Native object-list record 0 at EWRAM `0x02026DAC` has type `0x006C`, **16×32 source size**, actor animation 3. R86 source `source/field68.c` and `source/field74.c` show deferred sprite VRAM publication; static dimensions alone do not prove original Genesis source equality.

**Interpretation:** This scene already presents 16×32 native-pixel GBA sprites; globally enlarging them or scaling the field would harm existing alignment. The next stage must identify the exact original Genesis actor 0x006C pose/tiles/animation mapping before replacing pixels. No overworld graphics have been changed or claimed improved in Stage11.

Source/QA kit contains a reusable local extractor and the two decoded sprite images. No commercial sprite data is checked into this public repository.