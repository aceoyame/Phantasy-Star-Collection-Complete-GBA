#!/usr/bin/env python3
"""Build a 32 MiB four-game Phantasy Star Collection image.

This packer preserves the original 8 MiB PSC image, redirects its three
EEPROM_V120 library copies to the cartridge SRAM window, adds a fourth launcher
selection, and appends the independently-linked PSIV module at file offset
0x00800000 / ROM address 0x08800000.

Physical save layout (32 KiB SRAM):
  0x0000..0x1fff  original PSC logical 8 KiB EEPROM image
  0x2000..0x3fff  PSIV logical 8 KiB save image
  0x4000..0x7fff  reserved
"""
from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

PSC_SIZE = 0x00800000
PSIV_SLOT_SIZE = 0x01800000
COMBINED_SIZE = 0x02000000
PSIV_ROM_ADDR = 0x08800000

# Stock launcher selection state is at 0x0202436c.  These two Thumb immediates
# are the lower/upper wrap constants; changing 2 -> 3 admits selection index 3
# without touching the stock three-card drawing loops.
LAUNCHER_WRAP_PATCHES = {
    0x00000698: (bytes.fromhex("02 20"), bytes.fromhex("03 20")),  # movs r0,#2 -> #3
    0x0000069E: (bytes.fromhex("02 28"), bytes.fromhex("03 28")),  # cmp r0,#2 -> #3
}
LAUNCH_TABLE = 0x0001883C
LAUNCH_INDEX4 = LAUNCH_TABLE + 3 * 4
EXPECTED_FOURTH_OLD = 0x087F8000

# R13 slot-4 warm-launch handoff.
#
# Root cause of the R5..R12 four-game Mode-3/4 "black/solid colour" failure:
# the stock PSC launcher, immediately before `bx r0` into ANY game
# (0x08000752..0x08000770, then 0x080008BC), writes 0 to BG2X, BG2Y,
# BG2PA, BG2PB, BG2PC, BG2PD, BLDCNT and BLDALPHA.  PSC's own games program
# their affine unit themselves (e.g. PSI writes BG2PA=0x0111/BG2PD=0x0125 at
# 0x087386E2).  PSIV never writes BG2PA..BG2PD: it relies on the power-on /
# BIOS identity matrix.  In bitmap modes 3/4/5 BG2 is always sampled through
# the affine unit, so PA=PB=PC=PD=0 makes every screen pixel read texel (0,0):
# every Mode-4 scene (fading-text intro, title, start menu, AW2284 interlude,
# manga panels, battles, pause) collapses to one solid colour while Mode-0
# field/dialogue renders normally.  Cold boot gets identity from reset.
#
# Launch-table entry 3 therefore points at this ARM trampoline (entered by the
# launcher's `bx r0` with an even address -> ARM state).  It restores exactly
# the reset/BIOS affine state for BG2 and BG3 and then branches to the
# unchanged PSIV module at 0x08800000.  It clobbers only r0/r1, which PSIV's
# _start never reads.  The PSIV module bytes are NOT modified, so the R12
# relocated cold-boot ROM remains a valid byte-identical control.
SLOT4_HANDOFF_OFF = 0x00041800
SLOT4_HANDOFF_ADDR = 0x08000000 + SLOT4_HANDOFF_OFF


def arm_b(src: int, dst: int) -> int:
    off = (dst - (src + 8)) >> 2
    if not -(1 << 23) <= off < (1 << 23):
        raise SystemExit("ARM B out of range")
    return 0xEA000000 | (off & 0xFFFFFF)


def slot4_handoff_code(entry: int = 0x08800000) -> bytes:
    words = [
        0xE3A00301,  # mov   r0,#0x04000000
        0xE3A01C01,  # mov   r1,#0x100
        0xE1C012B0,  # strh  r1,[r0,#0x20]   BG2PA = 0x0100
        0xE1C012B6,  # strh  r1,[r0,#0x26]   BG2PD = 0x0100
        0xE1C013B0,  # strh  r1,[r0,#0x30]   BG3PA = 0x0100
        0xE1C013B6,  # strh  r1,[r0,#0x36]   BG3PD = 0x0100
        0xE3A01000,  # mov   r1,#0
        0xE1C012B2,  # strh  r1,[r0,#0x22]   BG2PB = 0
        0xE1C012B4,  # strh  r1,[r0,#0x24]   BG2PC = 0
        0xE5801028,  # str   r1,[r0,#0x28]   BG2X  = 0
        0xE580102C,  # str   r1,[r0,#0x2C]   BG2Y  = 0
        0xE1C013B2,  # strh  r1,[r0,#0x32]   BG3PB = 0
        0xE1C013B4,  # strh  r1,[r0,#0x34]   BG3PC = 0
        0xE5801038,  # str   r1,[r0,#0x38]   BG3X  = 0
        0xE580103C,  # str   r1,[r0,#0x3C]   BG3Y  = 0
    ]
    words.append(arm_b(SLOT4_HANDOFF_ADDR + 4 * len(words), entry))
    return b"".join(struct.pack("<I", w) for w in words)

# The stock launcher has only three card BG planes.  Slot 4 therefore
# virtualizes BG3: PS III scrolls completely out, the hidden BG3 payload is
# swapped to a native PS IV card, then the same plane scrolls back in.  This
# keeps PSC's stock card animation/loader path and eliminates the old tiny OBJ
# "IV" marker/flicker workaround.  Hook + card payload live only in verified
# zero padding inside the launcher partition.
LAUNCH_VISUAL_CALL = 0x000006A8
LAUNCH_VISUAL_CALL_OLD = bytes.fromhex("0C 68 80 27")  # ldr r4,[r1]; movs r7,#0x80
LAUNCH_VISUAL_CALL_NEW = bytes.fromhex("40 F0 AA FC")  # bl 0x08041000
LAUNCH_HOOK_OFF = 0x00041000
LAUNCH_HOOK_PATH = Path(__file__).resolve().parent.parent / "gba" / "psc4_launcher_hook.bin"
PSC4_CARD_GFX_OFF = 0x00042000
PSC4_CARD_MAP_OFF = 0x00043600
PSC4_CARD_PAL_OFF = 0x00043B00
PSC4_CARD_GFX_PATH = Path(__file__).resolve().parent.parent / "gba" / "psc4_card_gfx.bin"
PSC4_CARD_MAP_PATH = Path(__file__).resolve().parent.parent / "gba" / "psc4_card_map.bin"
PSC4_CARD_PAL_PATH = Path(__file__).resolve().parent.parent / "gba" / "psc4_card_pal.bin"

# P17 uses two palette entries that the retail PSIII launcher card never references.
# Patch only those unused slots so PSC's normal BG3 palette reassertion produces
# the same palette the PSIV card was authored against; PSIII pixels are unchanged.
PSC3_PAL_UNUSED_OFF = 0x00017C42  # PSIII palette entries 14 and 15
PSC3_PAL_UNUSED_OLD = bytes.fromhex("b556b556")
PSC3_PAL_UNUSED_NEW = bytes.fromhex("df43bd0e")  # pale gold, orange

# Extend the retail PSC launcher medley with a fourth M4A/Sappy section.
# Only the four original backward-GOTO targets are redirected.  Each extension
# uses the same retail voicegroup/instruments, runs exactly 1,320 ticks, then
# jumps to the exact original loop point.  Original I/II/III music bytes remain
# unchanged apart from these four pointer words.
PSC4_MENU_MUSIC_EXT = [
    # (retail GOTO pointer field, expected old target, extension file offset, asset)
    (0x000401C9, 0x0803FF6E, 0x00046800, "psc4_menu_track0_ext.bin"),
    (0x00040353, 0x080401D0, 0x00046C00, "psc4_menu_track1_ext.bin"),
    (0x000404D5, 0x0804035A, 0x00047000, "psc4_menu_track2_ext.bin"),
    (0x000407B1, 0x080404DC, 0x00047400, "psc4_menu_track3_ext.bin"),
]

# Reuse only verified trailing-zero padding in the original PSC partitions.
# These are storage overlays for PSIV cold immutable banks; no original game
# code/data is relocated and all three original launch addresses stay fixed.
PSC_PANEL_HOLE_OFF = 0x002D3B80
PSC_PANEL_HOLE_END = 0x00390000
PSC_PANEL_BANK_PATH = Path(__file__).resolve().parent.parent / "gba" / "psc_panel_bank.bin"
PSC_ENEMY_HOLE_OFF = 0x006CA4EC
PSC_ENEMY_HOLE_END = 0x00738000
PSC_ENEMY_BANK_PATH = Path(__file__).resolve().parent.parent / "gba" / "source_enemy_effects.bin"

# GBATA-compatible EEPROM_V120/121/122 patches, independently visible in all
# three native PSC partitions.  The first routine copies 8 bytes SRAM->caller;
# the second copies caller->SRAM.  The EEPROM block number in r0 is converted to
# byte offset block*8, preserving each game's existing logical block layout.
READ_MARKER = bytes([
    0xA2,0xB0,0x0D,0x1C,0x00,0x04,0x03,0x0C,0x03,0x48,0x00,0x68,
    0x80,0x88,0x83,0x42,0x05,0xD3,0x01,0x48,
])
READ_SRAM = bytes([
    0x00,0x04,0x0A,0x1C,0x40,0x0B,0xE0,0x21,0x09,0x05,0x41,0x18,
    0x07,0x31,0x00,0x23,0x08,0x78,0x10,0x70,0x01,0x33,0x01,0x32,
    0x01,0x39,0x07,0x2B,0xF8,0xD9,0x00,0x20,0x70,0xBC,0x02,0xBC,
    0x08,0x47,
])
PROGRAM_MARKER = bytes([
    0x30,0xB5,0xA9,0xB0,0x0D,0x1C,0x00,0x04,0x04,0x0C,0x03,0x48,
    0x00,0x68,0x80,0x88,0x84,0x42,0x05,0xD3,0x01,0x48,
])
PROGRAM_SRAM = bytes([
    0x70,0xB5,0x00,0x04,0x0A,0x1C,0x40,0x0B,0xE0,0x21,0x09,0x05,
    0x41,0x18,0x07,0x31,0x00,0x23,0x10,0x78,0x08,0x70,0x01,0x33,
    0x01,0x32,0x01,0x39,0x07,0x2B,0xF8,0xD9,0x00,0x20,0x70,0xBC,
    0x02,0xBC,0x08,0x47,
])
EEPROM_MAGIC = b"EEPROM_V120"
SRAM_MAGIC_PAD = b"SRAM_V110\0\0"  # exactly 11 bytes


def find_all(blob: bytes | bytearray, needle: bytes) -> list[int]:
    out: list[int] = []
    pos = 0
    while True:
        pos = blob.find(needle, pos)
        if pos < 0:
            return out
        out.append(pos)
        pos += 1


def patch_all(blob: bytearray, marker: bytes, replacement: bytes, expected: int, label: str) -> list[int]:
    hits = find_all(blob, marker)
    if len(hits) != expected:
        raise SystemExit(f"{label}: expected {expected} matches, found {len(hits)}: {[hex(x) for x in hits]}")
    for off in hits:
        blob[off:off+len(replacement)] = replacement
    return hits


def gba_complement(rom: bytes | bytearray) -> int:
    chk = 0
    for b in rom[0xA0:0xBD]:
        chk = (chk - b) & 0xFF
    return (chk - 0x19) & 0xFF


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--psc", required=True, type=Path, help="original 8 MiB PSC ROM")
    ap.add_argument("--psiv", required=True, type=Path, help="relocated raw PSIV module linked at 0x08800000")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--raw-out", type=Path, help="optional trimmed combined image before 32 MiB padding")
    ap.add_argument("--no-overlays", action="store_true", help="CP22 unified build: full PSIV banks are inside the module; leave PSC padding untouched")
    ap.add_argument("--print-sha", action="store_true", help="print SHA-256 only for a final checkpoint")
    ap.add_argument("--slot4-direct", action="store_true",
                    help="R12 behaviour: launcher entry 3 jumps straight to 0x08800000 with PSC's zeroed BG2 affine matrix (diagnostic/regression only)")
    args = ap.parse_args()

    psc = bytearray(args.psc.read_bytes())
    psiv = args.psiv.read_bytes()
    if len(psc) != PSC_SIZE:
        raise SystemExit(f"PSC must be exactly 8 MiB, got {len(psc):,} bytes")
    if len(psiv) > PSIV_SLOT_SIZE:
        raise SystemExit(f"PSIV module exceeds hard 24 MiB slot: {len(psiv):,} > {PSIV_SLOT_SIZE:,}")
    if len(psiv) < 4:
        raise SystemExit("PSIV module is too small")

    # A relocated GBA module still starts with an ARM branch header instruction.
    first = struct.unpack_from("<I", psiv, 0)[0]
    if (first & 0x0F000000) != 0x0A000000:
        raise SystemExit(f"PSIV module does not start with an ARM B/BL header opcode: 0x{first:08X}")

    # Launcher selection wrap + fourth dispatch pointer.
    for off, (before, after) in LAUNCHER_WRAP_PATCHES.items():
        if psc[off:off+len(before)] != before:
            raise SystemExit(f"launcher patch mismatch at 0x{off:X}")
        psc[off:off+len(after)] = after
    old_target = struct.unpack_from("<I", psc, LAUNCH_INDEX4)[0]
    if old_target != EXPECTED_FOURTH_OLD:
        raise SystemExit(f"unexpected launcher index-3 target 0x{old_target:08X}")
    if args.slot4_direct:
        slot4_target = PSIV_ROM_ADDR
        handoff = b""
    else:
        handoff = slot4_handoff_code(PSIV_ROM_ADDR)
        if any(psc[SLOT4_HANDOFF_OFF:SLOT4_HANDOFF_OFF+len(handoff)]):
            raise SystemExit("slot-4 handoff cave is no longer zero/owned")
        psc[SLOT4_HANDOFF_OFF:SLOT4_HANDOFF_OFF+len(handoff)] = handoff
        slot4_target = SLOT4_HANDOFF_ADDR
    struct.pack_into("<I", psc, LAUNCH_INDEX4, slot4_target)

    # Fourth-card presentation hook.  Never consume an unverified binary hole.
    hook = LAUNCH_HOOK_PATH.read_bytes()
    if psc[LAUNCH_VISUAL_CALL:LAUNCH_VISUAL_CALL+4] != LAUNCH_VISUAL_CALL_OLD:
        raise SystemExit("launcher visual hook call-site mismatch")
    if any(psc[LAUNCH_HOOK_OFF:LAUNCH_HOOK_OFF+len(hook)]):
        raise SystemExit("launcher visual hook cave is no longer zero/owned")
    psc[LAUNCH_VISUAL_CALL:LAUNCH_VISUAL_CALL+4] = LAUNCH_VISUAL_CALL_NEW
    psc[LAUNCH_HOOK_OFF:LAUNCH_HOOK_OFF+len(hook)] = hook

    # Real PS IV launcher card.  The stock card loader consumes the same
    # compressed-tiles + 30x20 map + 16-color palette shape used by PS I-III.
    # Keep each payload in a separately verified launcher-tail zero range.
    card_parts = [
        ("PSIV launcher card gfx", PSC4_CARD_GFX_OFF, PSC4_CARD_GFX_PATH.read_bytes()),
        ("PSIV native BG3 card map", PSC4_CARD_MAP_OFF, PSC4_CARD_MAP_PATH.read_bytes()),
        ("PSIV native BG3 card pal", PSC4_CARD_PAL_OFF, PSC4_CARD_PAL_PATH.read_bytes()),
    ]
    for label, off, data in card_parts:
        if any(psc[off:off+len(data)]):
            raise SystemExit(f"{label}: launcher padding at 0x{off:X} is no longer zero/owned")
        psc[off:off+len(data)] = data

    # Extend only the two PSIII palette entries proven unused by the retail
    # launcher card.  This does not alter any visible PSIII card pixel.
    if psc[PSC3_PAL_UNUSED_OFF:PSC3_PAL_UNUSED_OFF+4] != PSC3_PAL_UNUSED_OLD:
        raise SystemExit("unexpected PSIII unused palette slots")
    psc[PSC3_PAL_UNUSED_OFF:PSC3_PAL_UNUSED_OFF+4] = PSC3_PAL_UNUSED_NEW

    # Add PSIV to the stock PSC launcher medley without replacing the existing
    # music or driver.  The retail M4A tracks all reach their backward GOTO at
    # tick 4,800; redirect each to a same-instrument extension, which returns to
    # the original target after exactly 1,320 more ticks.
    music_parts = []
    gba_dir = Path(__file__).resolve().parent.parent / "gba"
    for ptr_off, old_target, ext_off, name in PSC4_MENU_MUSIC_EXT:
        got = struct.unpack_from("<I", psc, ptr_off)[0]
        if got != old_target:
            raise SystemExit(f"PSC menu music GOTO mismatch at 0x{ptr_off:X}: 0x{got:08X}")
        data = (gba_dir / name).read_bytes()
        if len(data) > 0x400:
            raise SystemExit(f"PSC menu music extension {name} exceeds 1 KiB slot")
        if any(psc[ext_off:ext_off+0x400]):
            raise SystemExit(f"PSC menu music extension slot 0x{ext_off:X} is no longer zero/owned")
        psc[ext_off:ext_off+len(data)] = data
        struct.pack_into("<I", psc, ptr_off, 0x08000000 + ext_off)
        music_parts.append((name, ptr_off, ext_off, len(data)))

    # Rewrite all three native games' EEPROM library boundary to SRAM.
    read_hits = patch_all(psc, READ_MARKER, READ_SRAM, 3, "ReadEepromDword")
    prog_hits = patch_all(psc, PROGRAM_MARKER, PROGRAM_SRAM, 3, "ProgramEepromDword")

    # Force save-type detection to SRAM.  Every embedded EEPROM marker becomes
    # the same SRAM marker; no EEPROM_V120 string remains in the outer image.
    magic_hits = find_all(psc, EEPROM_MAGIC)
    if len(magic_hits) != 3:
        raise SystemExit(f"expected 3 EEPROM_V120 markers, found {len(magic_hits)}")
    for off in magic_hits:
        psc[off:off+len(EEPROM_MAGIC)] = SRAM_MAGIC_PAD
    if EEPROM_MAGIC in psc:
        raise SystemExit("EEPROM marker survived rewrite")

    # Reclaim stock PSC padding without moving or deleting any of PS I/II/III.
    # Refuse to build if the commercial image no longer has the exact zero
    # caves this layout depends on.
    panel_bank = b"" if args.no_overlays else PSC_PANEL_BANK_PATH.read_bytes()
    enemy_bank = b"" if args.no_overlays else PSC_ENEMY_BANK_PATH.read_bytes()
    overlays = [] if args.no_overlays else [
        ("PSC panel bank", PSC_PANEL_HOLE_OFF, PSC_PANEL_HOLE_END, panel_bank),
        ("PSC source enemy bank", PSC_ENEMY_HOLE_OFF, PSC_ENEMY_HOLE_END, enemy_bank),
    ]
    for label, off, end, bank in overlays:
        if off + len(bank) > end:
            raise SystemExit(f"{label} does not fit verified PSC padding: {len(bank):,} > {end-off:,}")
        if any(psc[off:end]):
            raise SystemExit(f"{label}: PSC padding 0x{off:X}..0x{end-1:X} is not all zero")
        psc[off:off+len(bank)] = bank

    raw = bytes(psc) + psiv
    if len(raw) > COMBINED_SIZE:
        raise SystemExit(f"combined raw image exceeds 32 MiB: {len(raw):,}")
    if args.raw_out:
        args.raw_out.parent.mkdir(parents=True, exist_ok=True)
        args.raw_out.write_bytes(raw)

    # The outer PSC header remains authoritative.  Recompute its complement
    # anyway so future title/game-code edits cannot leave a stale header.
    out = bytearray(raw)
    out.extend(b"\xFF" * (COMBINED_SIZE - len(out)))
    out[0xBD] = gba_complement(out)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(out)

    print(f"PASS PSC EEPROM read hooks: {[hex(x) for x in read_hits]}")
    print(f"PASS PSC EEPROM program hooks: {[hex(x) for x in prog_hits]}")
    print(f"PASS PSC save markers -> SRAM: {[hex(x) for x in magic_hits]}")
    if handoff:
        print(f"PASS launcher index 3 -> slot-4 affine handoff 0x{SLOT4_HANDOFF_ADDR:08X} ({len(handoff)} bytes) -> 0x{PSIV_ROM_ADDR:08X}")
    else:
        print(f"PASS launcher index 3 -> 0x{PSIV_ROM_ADDR:08X} (direct; PSC-zeroed BG2 affine inherited)")
    print(f"PASS launcher slot-4 visual hook: {len(hook)} bytes at 0x{LAUNCH_HOOK_OFF:X}")
    print("PASS launcher slot-4 real BG3 card: " + ", ".join(f"{label} {len(data):,} @ 0x{off:X}" for label, off, data in card_parts))
    print("PASS PSC menu PSIV medley extension: " + ", ".join(f"{name} {n} bytes @ 0x{ext:X}" for name, _ptr, ext, n in music_parts))
    print("PASS PSIII unused palette slots 14/15 -> P17 PSIV gold highlights")
    if args.no_overlays:
        print("PASS PSC padding untouched: full PSIV manga/enemy banks are resident in the module")
    else:
        print(f"PASS PSC cold-bank overlay: panel {len(panel_bank):,} bytes @ 0x{PSC_PANEL_HOLE_OFF:X}")
        print(f"PASS PSC cold-bank overlay: enemy {len(enemy_bank):,} bytes @ 0x{PSC_ENEMY_HOLE_OFF:X}")
    print(f"PSIV module bytes: {len(psiv):,} / {PSIV_SLOT_SIZE:,} (headroom {PSIV_SLOT_SIZE-len(psiv):,})")
    print(f"combined raw bytes: {len(raw):,} / {COMBINED_SIZE:,} (headroom {COMBINED_SIZE-len(raw):,})")
    print(f"final padded bytes: {len(out):,}")
    print(f"header complement: 0x{out[0xBD]:02X}")
    if args.print_sha:
        print(f"sha256: {hashlib.sha256(out).hexdigest()}")


if __name__ == "__main__":
    main()
