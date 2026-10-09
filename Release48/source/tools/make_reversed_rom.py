#!/usr/bin/env python3
from pathlib import Path
import argparse, hashlib, os
ap=argparse.ArgumentParser(description='Create PSC-style descending PSIV ROM image used by the GBA recompiler runtime.')
ap.add_argument('rom',type=Path,help='bit-perfect 0x300000-byte PSIV ROM / ps4built.bin')
ap.add_argument('-o','--out',type=Path,default=Path('gba/ps4_reversed.bin'))
a=ap.parse_args(); b=a.rom.read_bytes()
if len(b)!=0x300000: raise SystemExit(f'expected 0x300000 bytes, got 0x{len(b):X}')
a.out.parent.mkdir(parents=True,exist_ok=True); a.out.write_bytes(b[::-1])
if os.environ.get('PS4_NO_HASH')!='1': print('source sha256',hashlib.sha256(b).hexdigest())
print('wrote',a.out,len(b),'bytes')
