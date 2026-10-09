from pathlib import Path
import argparse, hashlib
ap=argparse.ArgumentParser(); ap.add_argument('raw',type=Path); ap.add_argument('out',type=Path); a=ap.parse_args()
b=bytearray(a.raw.read_bytes())
# Keep a conventional power-of-two cart image; v0.9.6 uses nonblocking AOT v18 PCM.
for size in (4,8,16,32):
    cap=size*1024*1024
    if len(b)<=cap:
        break
else:
    raise SystemExit('raw image exceeds 32 MiB GBA maximum')
b.extend(b'\xff'*(cap-len(b)))
b[0xBD]=(-sum(b[0xA0:0xBD])-0x19)&0xff
a.out.write_bytes(b)
print('cart_mib',size)
print('sha256',hashlib.sha256(b).hexdigest())
print('header checksum',hex(b[0xBD]))
