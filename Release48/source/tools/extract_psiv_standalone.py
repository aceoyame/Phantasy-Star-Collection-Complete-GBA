#!/usr/bin/env python3
"""Extract the PSIV game from a PSC four-game ROM as a standalone GBA ROM.

The PSIV module is linked at 0x08800000 and is kept byte-identical: the
standalone image replaces the 8 MiB PSC region with a stub whose GBA header is
the module's own (logo, title PS4RC098, code P498) except word 0, which becomes
`B 0x08800000`.  The module's first word then branches to its _rom_start.  No
relink, no second build: the extracted ROM runs exactly the code the PSC4 image
runs.  Saves: the module addresses PSIV saves at SRAM+0x2000 in both forms, so a
PSC4 .sav works unchanged in the standalone ROM.  PS I-III bytes are not copied.
"""
import argparse, struct, sys
from pathlib import Path
MODULE_OFF = 0x800000
STUB_BRANCH = 0xEA000000 | (((0x08800000 - (0x08000000 + 8)) >> 2) & 0xFFFFFF)   # 0xEA1FFFFE
def complement(h):
    return (-sum(h[0xA0:0xBD]) - 0x19) & 0xFF
def extract(combined: bytes, pad_fill=0xFF) -> bytes:
    if len(combined) <= MODULE_OFF + 0xC0: raise SystemExit('image too small to contain a PSIV module')
    mod = combined[MODULE_OFF:]
    w0 = struct.unpack_from('<I', mod, 0)[0]
    if (w0 >> 24) != 0xEA or mod[0xA0:0xAC] != b'PS4RC098    ' or mod[0xAC:0xB0] != b'P498':
        raise SystemExit('no PSIV module header at 0x800000 (expected B + PS4RC098/P498)')
    stub = bytearray([pad_fill]) * MODULE_OFF
    stub[0:0xC0] = mod[0:0xC0]
    struct.pack_into('<I', stub, 0, STUB_BRANCH)
    stub[0xBD] = complement(stub)
    return bytes(stub) + mod
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('combined', type=Path, help='PSC4 four-game ROM (or a 0x08800000 module prefixed by 8 MiB)')
    ap.add_argument('-o', '--out', type=Path, required=True)
    a = ap.parse_args()
    out = extract(a.combined.read_bytes())
    a.out.write_bytes(out)
    print(f'PASS standalone PSIV: {len(out):,} bytes, entry B 0x08800000 -> module, header complement 0x{out[0xBD]:02X}')
if __name__ == '__main__': main()
