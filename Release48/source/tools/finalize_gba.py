#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib
ap=argparse.ArgumentParser(); ap.add_argument('raw',type=Path); ap.add_argument('out',type=Path); a=ap.parse_args()
b=bytearray(a.raw.read_bytes()); size=4*1024*1024
if len(b)>size: raise SystemExit('raw image exceeds 4 MiB')
b.extend(b'\xff'*(size-len(b))); b[0xBD]=(-sum(b[0xA0:0xBD])-0x19)&0xff
a.out.write_bytes(b); print('sha256',hashlib.sha256(b).hexdigest()); print('header checksum',hex(b[0xBD]))
