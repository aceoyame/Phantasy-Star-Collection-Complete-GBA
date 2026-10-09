import sys, struct
NOKEYS = False
from bench import GBA, CASES
def shots(rom, name, marks):
    g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    out = {}; t = 0
    # inputs are applied by guest frame number so both builds see the same input at the same guest frame
    while len(out) < len(marks) and t < 20000:
        gf = g.rd32(0x0203e004)
        keys = 0 if NOKEYS else ('START' if gf % 240 in range(120, 126) else ('A' if gf % 240 in range(200, 204) else 0))
        g.run(1, keys); t += 1
        gf2 = g.rd32(0x0203e004)
        for m in marks:
            if m not in out and gf2 >= m:
                out[m] = (gf2, list(g.shot().getdata()))
    return out
NOKEYS = len(sys.argv) > 5
marks = [int(x) for x in sys.argv[4].split(',')]
a = shots(sys.argv[1], sys.argv[3], marks); b = shots(sys.argv[2], sys.argv[3], marks)
for m in marks:
    if m in a and m in b:
        fa, pa = a[m]; fb, pb = b[m]
        diff = sum(1 for x, y in zip(pa, pb) if x != y)
        print(f'mark {m}: frames {fa}/{fb} differing pixels {diff}')
