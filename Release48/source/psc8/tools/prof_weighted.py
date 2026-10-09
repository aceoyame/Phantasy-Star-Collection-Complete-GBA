import sys, struct, subprocess, bisect, collections
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA, L
sys.path.insert(0,'/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/psc8/tools')
from bench import CASES
rom, elf, name, f0, f1 = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
import os
if os.environ.get('MONKEY'):                      # random-button script (tools/bench.py monkey), f0 display frames of warm-up
    from bench import monkey
    mk = monkey(int(os.environ['MONKEY'])); t = 0
    while t < f0:
        n, k = next(mk); g.run(n, k); t += n
    n, k = next(mk); g.run(n, k)
else:
  for t in range(f0):
    g.run(1, 'START' if t % 240 in (120,121,122) else ('A' if t%240 in (200,201) else 0))
g.pprof(int(sys.argv[6]) if len(sys.argv)>6 else 4000000)
d = g.pprof_dump()
syms = []
for line in subprocess.run(['llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [s[0] for s in syms]
acc = collections.Counter(); tot = 0; rom_i=0;all_i=0
for a, c in d:
    i = bisect.bisect_right(addrs, a) - 1
    w = 4 if a >> 24 >= 8 else 1
    acc[syms[i][1] if i >= 0 else hex(a)] += c*w; tot += c*w; all_i+=c; rom_i+= c if a>>24>=8 else 0
print('instr in ROM: %.1f%%  (weighted time shares, ROM x4)'%(100*rom_i/all_i))
for k, v in acc.most_common(22): print(f'{100*v/tot:6.2f}% {k}')
