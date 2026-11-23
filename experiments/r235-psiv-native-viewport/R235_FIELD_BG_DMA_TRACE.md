# R235 Academy background scroll DMA attribution

**Experiment status: read-only live instrumentation; no new alternate viewport implemented.**

Accepted R235: `PSC_R235_FIXB_NEWGAME_RELEASE.gba`, 33,554,432 bytes, SHA256 `4caf23ae8b030c16a32658fdc95085330de6c6dd4a51498f0e2b9c17c7d89308`.

## Identified live ARM routine
The executed ARM function beginning at **`0x08C7D124`** (entry visible at `0x08C7D12C` after prologue) prepares GBA background scrolling:

- `0x08C7D12C`: loads a live RAM flag
- `0x08C7D130-0x08C7D174`: chooses two alternating source arrays, writes eight BG scroll values to I/O, then selects the HBlank DMA scroll buffer
- **`0x08C7D17C`**: `str r2, [r0,#0xA0]` with live `r0=0x04000010`. This sets `DMA0SAD` at **`0x040000B0`**. The actual observed source pointer alternates between `0x02034010` and `0x02034A20`.
- **`0x08C7D180`**: `str r0,[r0,#0xA4]`, sets `DMA0DAD = 0x04000010` (BG0 horizontal scroll).
- **`0x08C7D184`**: `str r1,[r0,#0xA8]` enables DMA with `r1=0xA2600008` (8 halfwords per HBlank; source increments, destination reloads).

The alternating EWRAM regions **`0x02034000`** and **`0x02034A10`** each contain a screen's scroll data for BG0–3. A live breakpoint at `0x08C7D17C` hit during Academy movement and exposed both the exact CPU PC and source pointer.

## Registered movement examples

Restored real accepted-R235 Academy state, map 18:

| Frame | Player X | Player Y | First scroll buffer X/Y |
|---|---:|---:|---|
| 16455 | 72 | 232 | `0x02034000: 328,26`; `0x02034A10: 72,232` |
| 16485 | 132 | 232 | `0x02034000: 130,232`; `0x02034A10: 132,232` |
| 16495 | 136 | 232 | `0x02034000: 134,232`; `0x02034A10: 136,232` |
| 16520 | 168 | 232 | `0x02034000: 166,232`; `0x02034A10: 168,232` |

Both buffers repeat interleaved X/Y halfwords across the majority of 0xA10-byte regions. This is a double-buffered raster-scroll presentation path, **not** yet proof of the owner of scene/map tile publication.

## Related actor-only field diagnostic

In a separate **diagnostic-only** R235 build, adding 8 pixels to the actor-position argument at the previously verified field-camera shim changed exactly **885** pixels compared with baseline after 30 frames of RIGHT on the same Academy save state. The map and world-space player coordinate were unchanged; displaced pixels were concentrated around visible actors. The background was not shifted. Therefore, that actor-only diagnostic must **not** be promoted as an alternate camera.

## Integration gate

The correct next step is to identify where both 0xA10-byte scroll buffers are generated and adjust BG scrolling in lockstep with the sprite camera, with a runtime opt-in and frame-exact rollback. A safe patch cannot simply nudge the DMA output register after the fact, since the DMA overwrites the register every scanline and the buffers are regenerated during gameplay. Need to validate field transitions, indoor/outdoor effects, text overlay, battle mode, and real hardware.

This document contains no game assets or proprietary source.
