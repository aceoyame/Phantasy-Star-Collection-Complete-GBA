# CP10 addendum (CRC32 `e31bccaa`) — supersedes the CP09 numbers below where different

The Genesis runtime grew slightly (save code): first free interval is now `0x00723EF5..0x00730000` = **49,419 B** (was 50,387). Others unchanged:
`0x01FF809C..0x02000000` 32,612 B; `0x006DB558..0x006E2000` 27,304 B; `0x00731CD5..0x00738000` 25,387 B; `0x0000588D..0x00007F00` 9,843 B;
slot-IV padding `0x05F660..0x060000` 2,464 B and `0x07930C..0x07C000` 11,508 B (verified filler 00/FF in the CP10 ROM). Total ≈ 158 KB in separate intervals.
IWRAM: MD `.hot` ends `0x03007790`, limit `0x03007980` → **496 B spare**. SRAM map: adventures use `0x7000..0x73FF` (+ magic `0x7F00`).

---

# ROM and RAM budget — CP09 (CRC32 `34f14438`)

32 MiB ROM. File offsets; cartridge address = `0x08000000 +` offset; end exclusive. Re-run `source/tools/rom_space.py ROM` on any new build
(it does not know the CP09 padding allocations below, so read this file together with `build_artifacts/CP09_BUILD_REPORT.json`).

## Known free ROM (trailing filler after a blob, safe to allocate)

| File offsets | Cart address | Bytes | What it is |
| --- | --- | ---: | --- |
| `0x00723B2D..0x00730000` | `0x08723B2D` | **50,387** | After the Genesis (MD) runtime (269,104 B at `0x086E2000`); the PSII debug module starts at `0x730000` |
| `0x01FF809C..0x02000000` | `0x09FF809C` | **32,612** | ROM tail after the Nei pages and the 68000 AOT tables (erased, 0xFF) |
| `0x006DB558..0x006E2000` | `0x086DB558` | **27,304** | PSC8 slack (PSC8 = 69,720 B at `0x086CA500`; hard limit `0x086E2000`) |
| `0x00731CD5..0x00738000` | `0x08731CD5` | **25,387** | After the PSII debug module (7,382 B at `0x08730000`; `0x738000` is the PSII end-of-payload) |
| `0x0000588D..0x00007F00` | `0x0800588D` | **9,843** | After the Additional Content menu blob (2,189 B at `0x08005000`; trampoline at `0x7F00`) |
| **Total** | | **145,533 (142.1 KiB)** | separate intervals |

## CP09 allocations in the slot-IV padding (`0x4766D..0x80000`)

That range is zero padding between the PSIV entry stub and the PSIII image. 24 randomised 9,000-frame emulator runs of the collection,
PSII, PSIII and PSIV never read it (`tools/rom_read_trace.py`); it separates two images by alignment only. CP09 uses `0x48000..0x7C000` of it
(the build asserts the range is zero in the base):

| File offsets | Bytes | Content |
| --- | ---: | --- |
| `0x048000..0x05F660` | 95,840 | Genesis sound bank: index 20,760 B (`.sndidx`) + 843 wavetables 75,072 B |
| `0x05F660..0x060000` | **2,464 free** | |
| `0x060000..0x07930C` | 103,180 | Genesis sound engine (`mdsnd.c`, Thumb) incl. the translated Z80 driver (~48 KB) — linker section `.zaot` |
| `0x07930C..0x07C000` | **11,508 free** | still inside the read-trace-verified range |
| `0x07C000..0x080000` | (16,384) | left untouched (not covered by the CP09 assertion) |

## Not free (filler inside payloads — audit before use)

`0x7F2764..0x800000` (55,452 B, PSI-era tail, re-audit), `0x8A815C..0x8B3748` (46,572 B, PSIV area), `0x4EB9E4..0x4F4712` (36,142 B, PSII),
`0x50F8BB..0x516560` (27,813 B, PSII), `0x25E05E..0x264B84` (27,430 B, PSIII), `0x34E310..0x353B80` (22,640 B, tail of the PSI image), more via `rom_space.py --big N`.

## RAM

| Item | Value |
| --- | --- |
| MD runtime IWRAM | `.hot` ends `0x03007960`, link limit `0x03007980` → **32 B spare**; user stack top `0x03007F00` |
| PSC8 IWRAM | ends `0x03006FC4` |
| Sound engine RAM | EWRAM `.bss` (~16.7 KB); the renderer's 16-sample accumulators live on the IWRAM stack (192 B) |

## Save SRAM (32 KiB at `0x0E000000`)

| Range | Owner |
| --- | --- |
| `0x0000..0x1FFF` | PSC (main collection) |
| `0x2000..0x3FFF` | PSIV |
| `0x4000..0x5FFF` | PSI (lower half of PSI's block) |
| `0x6000..0x6FFF` | Phantasy Star Gaiden cartridge SRAM (first 4 KiB), magic `GGSV` at `0x7F10` |
| `0x7000..0x73FF` | Genesis adventures' 128-byte EEPROM images, slot = menu index − 2 (`0x7000 + slot*128`), magic `MDEE` at `0x7F00` |
