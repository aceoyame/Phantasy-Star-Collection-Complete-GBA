#!/usr/bin/env python3
"""Profile one tools/ubench.py case by symbol: uprof.py ROM ELF case [steps]"""
import sys, struct, subprocess, bisect, collections
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba'); sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/psc8/tools')
from mgba2 import GBA
import ubench
rom, elf, name = sys.argv[1:4]; steps = int(sys.argv[4]) if len(sys.argv) > 4 else 3000000
img, titer = ubench.case(name)
g = GBA(rom, entry=0x086ca500); g.put(ubench.CART, bytes(img) + bytes(0x8000 - len(img))); g.put(0x0203FF00, struct.pack('<IIII', ubench.CART, 0x8000, 0, 0))
g.run(40); g.pprof(steps); d = g.pprof_dump()
syms = []
for line in subprocess.run(['llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [s[0] for s in syms]; acc = collections.Counter(); tot = 0
for a, c in d:
    i = bisect.bisect_right(addrs, a) - 1; acc[syms[i][1] if i >= 0 else hex(a)] += c; tot += c
print(name, 'total cycles', tot)
for k, v in acc.most_common(14): print(f'{100*v/tot:6.2f}% {k}')
