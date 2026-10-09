#!/usr/bin/env python3
"""V-counter wait loops on the GBA core: exit line and H position for each loop form.  vwtest.py ROM"""
import sys, struct
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/psc8/tools')
from mgba2 import GBA
import ubench
PRE = ubench.PRE
cases = [('NZ', 10), ('C', 40), ('NZ', 100), ('Z', 100), ('C', 150), ('NZ', 191), ('C', 192), ('NZ', 200), ('NC', 200)]
OP = {'NZ': 0x20, 'Z': 0x28, 'NC': 0x30, 'C': 0x38}
def build(body_extra):
    code = [0x21, 0x00, 0xC1]                      # LD HL,0xC100
    for cc, n in cases:
        # l: [body_extra] IN A,(7E); CP n; JR cc,l
        loop = list(body_extra) + [0xDB, 0x7E, 0xFE, n, OP[cc], 0]
        loop[-1] = (-len(loop)) & 0xFF
        code += loop
        code += [0x77, 0x23, 0xDB, 0x7F, 0x77, 0x23]       # LD (HL),A ; INC HL ; IN A,(7F) ; LD (HL),A ; INC HL
    code += [0x18, 0x100 - (len(code) + 2) + 0]          # JR start (LD HL..)
    # JR back to the LD HL at the first code byte
    code[-1] = (-(len(code) )) & 0xFF
    img = bytearray(ubench.prog(PRE, code)); img += bytes(0x4000 - len(img))
    # fix JR target: it must jump to len(PRE)  -> the code starts at len(PRE); JR is the last 2 bytes
    jr_at = len(PRE) + len(code) - 2
    img[jr_at + 1] = (len(PRE) - (jr_at + 2)) & 0xFF
    return img
def run(rom, extra, label):
    img = build(extra)
    g = GBA(rom, entry=0x086ca500)
    g.put(ubench.CART, bytes(img) + bytes(0x8000 - len(img)))
    g.put(0x0203FF00, struct.pack('<IIII', ubench.CART, 0x8000, 0, 0))
    ram = 0x0200d0cc
    res = []
    for t in range(6):
        g.run(10)
        res.append(list(g.mem(ram + 0x100, 2 * len(cases))))
    print(label, 'stable' if all(r == res[-1] for r in res[1:]) else 'UNSTABLE')
    r = res[-1]
    for i, (cc, n) in enumerate(cases):
        print('  %-2s %3d -> V=%3d H=%3d' % (cc, n, r[2 * i], r[2 * i + 1]))
if __name__ == '__main__':
    run(sys.argv[1], [], 'plain loop')
    run(sys.argv[1], [0xDB, 0xDC, 0x2F, 0xE6, 0xA0], 'loop with key poll body (IN A,(DC); CPL; AND A0)')
