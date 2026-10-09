#!/usr/bin/env python3
from pathlib import Path
import argparse
ap=argparse.ArgumentParser(description='Extract the standard 156-byte GBA Nintendo-logo header block from an existing valid GBA ROM.')
ap.add_argument('gba',type=Path); ap.add_argument('-o','--out',type=Path,default=Path('gba/nintendo_logo.bin'))
a=ap.parse_args(); b=a.gba.read_bytes()
if len(b)<0xA0: raise SystemExit('input too small')
a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_bytes(b[4:0xA0]); print('wrote',a.out,0x9C,'bytes')
