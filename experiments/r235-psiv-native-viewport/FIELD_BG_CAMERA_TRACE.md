# Field BG observations from R235 Academy saved state

Using `PSC_R235_FIXB_NEWGAME_RELEASE.gba` (accepted hash) and `PSC_HEADLESS_EMULATOR_TOOLKIT_2026-11-16.zip` from the user's Library:

- mGBA restored the real after-principal Academy map18 checkpoint.
- Player pixel X moved from **72 to 132** during a 30-frame RIGHT hold; Y remained 232.
- Display mode stayed `0x1F40` (GBA BG tile mode 0 with all four BG layers and OBJ enabled).
- All four BG HOFS/VOFS values were `0xFFFC` after settling, `0xFFFE` after moving.
- The tracer logged 360 stores to BG scroll registers and 43,288 stores while DMA was active over those 30 frames. The core recorded no visible-scanline CPU BG-map writes in its first-level categories. This does **not** identify precise DMA sources or background tile publication routines.
- These facts make shifting only the field sprite camera unsafe. Both BG tile presentation and OBJ positions must be modified coherently. The 240×160 Academy artwork already matches the original tile pixel geometry.

Reproduce with `trace_field_background.py`. The next investigation is DMA source/destination and BG map-building function ownership at an actual camera-step boundary. No ROM modifications beyond the separate no-op camera canary are supported by this trace.
