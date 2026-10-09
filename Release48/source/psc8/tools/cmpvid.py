import sys, struct
from bench import GBA, CASES
def state_at(rom, name, n, keys_on):
    g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    while g.rd32(0x0203e004) < n:
        gf = g.rd32(0x0203e004)
        k = ('START' if gf % 240 in range(120, 126) else ('A' if gf % 240 in range(200, 204) else 0)) if keys_on else 0
        g.run(1, k)
    return b''.join(bytes(g.mem(b, l)) for b, l in ((0x05000000, 1024), (0x06000000, 0x18000), (0x07000000, 1024), (0x04000010, 8)))
name = sys.argv[3]
for n in [int(x) for x in sys.argv[4].split(',')]:
    a = state_at(sys.argv[1], name, n, False); b = state_at(sys.argv[2], name, n, False)
    print(name, n, 'video memory identical' if a == b else 'DIFF %d bytes' % sum(1 for x, y in zip(a, b) if x != y))
