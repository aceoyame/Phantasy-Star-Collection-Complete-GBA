#!/usr/bin/env python3
"""Migrate an existing 8 KiB PSC EEPROM save to the four-game 32 KiB SRAM layout."""
from __future__ import annotations
import argparse
from pathlib import Path

OLD = 0x2000
NEW = 0x8000
PSIV_BASE = 0x2000
PSIV_END = 0x4000

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    a=ap.parse_args()
    old=a.input.read_bytes()
    if len(old) != OLD:
        raise SystemExit(f"expected an 8192-byte PSC EEPROM save, got {len(old)}")
    out=bytearray(b"\xFF"*NEW)
    out[:OLD]=old
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_bytes(out)
    assert out[:OLD] == old
    assert out[PSIV_BASE:PSIV_END] == b"\xFF"*(PSIV_END-PSIV_BASE)
    print(f"PASS migrated PSC bytes 0x0000-0x1FFF unchanged; PSIV 0x2000-0x3FFF blank; output={len(out)}")
if __name__ == "__main__": main()
