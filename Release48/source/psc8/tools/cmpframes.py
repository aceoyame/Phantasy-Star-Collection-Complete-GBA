import sys, struct
from bench import GBA, CASES
def trace(rom, name, nmax):
    g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    out = {}; last = -1
    for t in range(12000):
        g.run(1)
        v = struct.unpack('<14I', g.mem(0x0203E000, 56))
        if v[1] != last:
            last = v[1]; out[v[1]] = (hex(v[2]), v[7], hex(v[11]), hex(v[12]), hex(v[13]), v[4])
        if last >= nmax: break
    return out
a = trace(sys.argv[1], sys.argv[3], int(sys.argv[4])); b = trace(sys.argv[2], sys.argv[3], int(sys.argv[4]))
for k in sorted(set(a) & set(b)):
    x, y = a[k], b[k]
    same = x[1:5] == y[1:5] and abs(int(x[0], 16) - int(y[0], 16)) <= 1
    if not same:
        print('frame', k, x, y); break
print('compared', len(set(a)&set(b)))
