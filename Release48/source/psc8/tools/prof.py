import sys, struct, subprocess, bisect, collections
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA, L
from bench import CASES
rom, elf, name = sys.argv[1], sys.argv[2], sys.argv[3]
g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
for t in range(int(sys.argv[4]) if len(sys.argv)>4 else 900):
    g.run(1, 'START' if t % 240 in (120,121,122) else ('A' if t%240 in (200,201) else 0))
g.pprof(int(sys.argv[5]) if len(sys.argv)>5 else 3000000)
d = g.pprof_dump()
syms = []
for line in subprocess.run(['llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [s[0] for s in syms]
acc = collections.Counter(); tot = 0
for a, c in d:
    i = bisect.bisect_right(addrs, a) - 1
    acc[syms[i][1] if i >= 0 else hex(a)] += c; tot += c
for k, v in acc.most_common(25): print(f'{100*v/tot:6.2f}% {k}')
