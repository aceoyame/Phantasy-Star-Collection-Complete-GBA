#!/usr/bin/env python3
"""Usage: bench.py ROM [case...]  |  bench.py monkey ROM [case...]\nPSC8 speed bench: boots the PSC8 entry directly with an Additional-Content style config and measures
guest frames per GBA frame over a scripted input sequence (title -> menus)."""
import sys, struct, json
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA, L
CASES = {'PSI': (0x082d3b80, 0x80000, 10), 'ADV': (137706368, 131072, 1), 'GAIDEN': (166669648, 262144, 1)}
def run(rom, name, frames=2400, shots=None):
    g = GBA(rom, entry=0x086ca500)
    g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    marks = []; t = 0; prev = 0
    while t < frames:
        if t % 240 == 120: g.run(4, 'START'); t += 4
        elif t % 240 == 200: g.run(4, 'A'); t += 4
        else: g.run(20); t += 20
        if t % 600 < 20:
            gf = g.rd32(0x0203e004); marks.append((t, gf - prev)); prev = gf
            if shots: g.shot(f'{shots}_{name}_{t}.png')
    v = struct.unpack('<22I', g.mem(0x0203E000, 88))
    return dict(case=name, display=t, guest=v[1], speed=round(v[1] / t, 3), late=v[6], fm_ports=v[3], probes=v[14], skips=v[15], lastfail=[v[16], v[17], hex(v[18]), hex(v[19])], replay_checks=v[20], replay_bad=v[21],
                per600=[m[1] for m in marks])
def monkey(seed=1):
    """endless (frames, key) steps of the random-button script"""
    KEYS = ['', '', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'B', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'A', 'B', '']
    st = seed; mt = 0; mk = ''
    while True:
        if mt == 0:
            st = (st * 1103515245 + 12345) & 0xffffffff; r = st >> 16; mt = 4 + (r % 24); mk = KEYS[(r >> 5) % 16]
            if r % 23 == 0: mk = 'START'
        n = min(mt, 8); mt -= n; yield n, mk
def run_monkey(rom, name, frames=12000, seed=1):
    """Random button presses (same generator as tools/sms_host.c 'script monkey'): reaches gameplay, reports guest speed per 1000
    display frames, so heavy scenes (map loads, battles) show up as slow windows."""
    KEYS = ['', '', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'B', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'A', 'B', '']
    g = GBA(rom, entry=0x086ca500)
    g.put(0x0203FF00, struct.pack('<IIII', *CASES[name], 0))
    st = seed; mk = ''; mt = 0; t = 0; prev = 0; win = []; late0 = 0; latew = []
    while t < frames:
        if mt == 0:
            st = (st * 1103515245 + 12345) & 0xffffffff; r = st >> 16; mt = 4 + (r % 24); mk = KEYS[(r >> 5) % 16]
            if r % 23 == 0: mk = 'START'
        n = min(mt, 8); g.run(n, mk); mt -= n; t += n
        if t // 1000 != (t - n) // 1000:
            v = struct.unpack('<14I', g.mem(0x0203E000, 56)); win.append(v[1] - prev); prev = v[1]; latew.append(v[6] - late0); late0 = v[6]
    v = struct.unpack('<22I', g.mem(0x0203E000, 88))
    return dict(case=name + '-monkey', seed=seed, display=t, guest=v[1], speed=round(v[1] / t, 3), late=v[6], probes=v[14], skips=v[15], replay_checks=v[20], replay_bad=v[21], per1000=win, late_per1000=latew)
if __name__ == '__main__':
    if sys.argv[1:2] == ['monkey']:
        for c in sys.argv[3:] or list(CASES): print(json.dumps(run_monkey(sys.argv[2], c)), flush=True)
        sys.exit()
    rom = sys.argv[1]; cases = sys.argv[2:] or list(CASES)
    for c in cases: print(json.dumps(run(rom, c, shots=None)), flush=True)
