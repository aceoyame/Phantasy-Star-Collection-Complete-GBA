#!/usr/bin/env python3
"""Usage: run_fmtest.py [fmtest.gba [header-rom]].  Runs build/fmtest.gba in the mGBA probe: 40 trials x 100 blocks of random YM2413 register writes, block renderer (assembly loops)
vs per-sample reference; result words at 0x0203E000: [0]=0x464D5431 when finished, [1]=samples, [2]=mismatches."""
import sys, struct
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA
rom = sys.argv[1] if len(sys.argv) > 1 else 'build/fmtest.gba'
hdr = sys.argv[2] if len(sys.argv) > 2 else '/home/claude/work/mdwork/m6.gba'   # any GBA ROM: mGBA only loads images with a valid cartridge header
img = bytearray(open(rom, 'rb').read())
if not any(img[:0xC0]): img[:0xC0] = open(hdr, 'rb').read(0xC0); open(rom + '.hdr.gba', 'wb').write(img); rom += '.hdr.gba'
g = GBA(rom, entry=0x080000C0)
for _ in range(4000):
    g.run(10)
    if g.rd32(0x0203E000) == 0x464D5431: break
r = struct.unpack('<3I', g.mem(0x0203E000, 12))
print('finished' if r[0] == 0x464D5431 else 'NOT FINISHED', 'samples', r[1], 'mismatches', r[2])
sys.exit(0 if r[0] == 0x464D5431 and r[2] == 0 else 1)
