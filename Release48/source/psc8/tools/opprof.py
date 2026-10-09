import sys, struct, subprocess, bisect, collections
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA
from bench import CASES
rom, elf = sys.argv[1], sys.argv[2]
syms = []
for line in subprocess.run(['llvm-nm', '-n', elf], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3: syms.append((int(p[0], 16), p[2]))
syms.sort(); addrs = [s[0] for s in syms]
tot = collections.Counter()
for name, warm in (('PSI', 1500), ('GAIDEN', 2100), ('ADV', 900), ('GAIDEN', 900)):
    g = GBA(rom, entry=0x086ca500); g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    for t in range(warm): g.run(1, 'START' if t % 240 in (120,121,122) else ('A' if t%240 in (200,201) else 0))
    g.pprof(2000000)
    for a, c in g.pprof_dump():
        i = bisect.bisect_right(addrs, a) - 1
        n = syms[i][1] if i >= 0 else '?'
        if n.startswith('op_') or n.startswith('xy_') or n.startswith('ed_'): tot[n] += c
print(' '.join(n for n, c in tot.most_common(70)))
