import sys, struct
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA
mode = int(sys.argv[1]) if len(sys.argv) > 1 else 0
rom = sys.argv[2] if len(sys.argv) > 2 else 'test.bin'
g = GBA(rom, entry=0x080000C0)
g.put(0x0203FF00, struct.pack('<I', mode))
for i in range(4000 if mode == 0 else 200000):
    g.run(60)
    if g.rd32(0x0203E000) in (0x4F505354, 0x5A455844): break
    if mode == 1 and i % 50 == 0:
        n = g.rd32(0x0203E008); print('progress', n, g.rd32(0x0203E004), flush=True)
r = [g.rd32(0x0203E000 + 4 * k) for k in range(16)]
print('result', hex(r[0]), r[1], r[2])
if mode == 0:
    for b in range(min(r[2], 40)):
        v = [g.rd32(0x0203E040 + 32 * b + 4 * k) for k in range(8)]
        print('p%d op %02x s%d mism=%x code=%08x pc a/c=%08x f a/c=%04x cyc a/c=%08x hl=%08x bc=%08x' % (v[0] >> 16, (v[0] >> 8) & 255, v[0] & 255, v[1], v[2], v[3], v[4], v[5], v[6], v[7]))
else:
    n = g.rd32(0x0203E008); print(bytes(g.mem(0x02034000, n)).decode('latin1'))
