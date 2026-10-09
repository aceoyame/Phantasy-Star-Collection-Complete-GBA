import sys, struct, subprocess, collections
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA
from bench import CASES
rom, elf, name, fn = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
for t in range(int(sys.argv[5]) if len(sys.argv)>5 else 900): g.run(1, 'START' if t % 240 in (120,121,122) else ('A' if t%240 in (200,201) else 0))
g.pprof(3000000); d = g.pprof_dump()
syms = {}
for line in subprocess.run(['llvm-nm', '-S', elf], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 4: syms[p[3]] = (int(p[0], 16), int(p[1], 16))
lo, sz = syms[fn]
tot = sum(c for a, c in d); sel = [(a, c) for a, c in d if lo <= a < lo + sz]
s = sum(c for a, c in sel); print(fn, f'{100*s/tot:.1f}%')
lines = collections.Counter()
addrs = [hex(a) for a, c in sel]
out = subprocess.run(['llvm-addr2line', '-e', elf] + addrs, capture_output=True, text=True).stdout.split('\n')
for (a, c), l in zip(sel, out): lines[l.split('/')[-1]] += c
for l, c in lines.most_common(18): print(f'{100*c/s:5.1f}% {l}')
