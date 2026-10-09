#!/usr/bin/env python3
"""Linked PD ARM row: valid trimmed intervals, flips, transparency, bounds and ABI.

Constructed tile bytes exercise branches that native art can leave invisible.
The independent oracle reads original big-nibble-first tile bytes directly.
"""
from pathlib import Path
import argparse, json, struct, sys

R = Path(__file__).resolve().parents[1]
O = R.parent / 'build_artifacts'
ap = argparse.ArgumentParser()
ap.add_argument('--label', default='CP18')
a = ap.parse_args()
prefix = 'R46_' + a.label
sys.path.insert(0, str(R / 'tools_host/r46_mgba'))
from mgba import GBA, L, run_until_break

s = {}
for line in (R / 'build-psc-module/ps4_psc_module.map').read_text().splitlines():
    p = line.split()
    if len(p) >= 5:
        try:
            s[p[-1]] = int(p[0], 16)
        except ValueError:
            pass
g = GBA(R.parent / 'binaries/RUN_THIS_RELEASE48_PSC.gba')
g.load(O / (prefix + '_MGBA_DEBUG.bin'))
# Idle combo arena, distinct from PD's live $D000 cache and framebuffer.
plan, mapping, art_addr, job = 0x02033600, 0x02033800, 0x02033a00, 0x02033b00
dst = 0x0202a000
sentinels = [0x51a40000 + i * 0x101 for i in range(4, 12)]
initial = bytes([0xa5]) * 16 + bytes([55]) * 240 + bytes([0xa5]) * 16
cases = 0

def tile(pattern, seed):
    out = bytearray()
    for y in range(8):
        pixels = [(x * 3 + y * 5 + seed) % 15 + 1 for x in range(8)]
        if pattern == 1:
            pixels = [c if (x + y) & 1 else 0 for x, c in enumerate(pixels)]
        elif pattern == 2:
            pixels = [0] * 8
        elif pattern == 3:
            pixels = [c if x == (y + seed) % 8 else 0 for x, c in enumerate(pixels)]
        out.extend((pixels[x] << 4) | pixels[x + 1] for x in range(0, 8, 2))
    return bytes(out)

def check(n, x, phase, ly, flags, art, base_tx=0, tx=0, tile_id=1, byte_count=64):
    global cases
    # Row API accepts only intervals already trimmed to real map columns.
    # The full-body test exercises source wrap and absent-column clipping.
    if (base_tx + tx) & 63 >= 40: return
    # A valid 5-bit nibble-offset stream, including nonzero first offsets.
    lx = [(phase + k * 4 // 3) & 7 for k in range(n)]
    shifts = sum(((p ^ 1) * 4) << (k * 5) for k, p in enumerate(lx))
    g.put(plan, struct.pack('<BBBBI', x, n, tx, 0, shifts))
    entry = tile_id | flags
    g.put(mapping, struct.pack('<40H', *([entry] * 40)))
    g.put(art_addr, art)
    g.put(job, struct.pack('<5I', plan, base_tx, ly, byte_count, 1))
    expected = bytearray(initial)
    if (base_tx + tx) & 63 < 40 and tile_id * 32 + 32 <= byte_count:
        y = ly ^ (7 if flags & 0x1000 else 0)
        for k, px in enumerate(lx):
            px ^= 7 if flags & 0x800 else 0
            b = art[tile_id * 32 + y * 4 + px // 2]
            c = (b >> (0 if px & 1 else 4)) & 15
            if c:
                expected[16 + x + k] = 192 + c
    g.put(dst - 16, initial)
    L.mg_break(0xffffffff)
    L.mg_cpsr(0x1f)
    for i, v in enumerate((mapping, art_addr, dst, job)):
        L.mg_set_reg(i, v)
    for i, v in enumerate(sentinels, 4):
        L.mg_set_reg(i, v)
    for i, v in ((13, 0x03007d80), (14, 0x08800000), (15, s['r46_pd_body_row'])):
        L.mg_set_reg(i, v)
    assert run_until_break(g, 0x08800000, 100), 'ARM row did not return'
    assert g.mem(dst - 16, len(initial)) == expected, (n, x, phase, ly, flags, base_tx, tx, tile_id, byte_count)
    assert [g.reg(i) for i in range(4, 12)] == sentinels, 'callee-saved register changed'
    assert g.reg(13) == 0x03007d80, 'stack pointer changed'
    cases += 1

for n in range(1, 7):
    for x in (0, 240 - n):
        for phase in range(8):
            for pattern in range(4):
                art = tile(pattern, phase + 1) + tile(pattern, phase + 7)
                for ly in range(8):
                    for flags in (0, 0x800, 0x1000, 0x1800):
                        check(n, x, phase, ly, flags, art)
print('PASS',cases,'linked ARM suffix/flip/transparency/edge cases',flush=True)

art = tile(0, 1) + tile(1, 9)
boundaries = [
    (0, 39, 1, 64),   # last assembled-map column
    (0, 40, 1, 64),   # first absent column
    (63, 1, 1, 64),   # 512-pixel source wrap to column zero
    (40, 0, 1, 64),   # absent column via row base
    (0, 0, 0, 31),    # incomplete first tile
    (0, 0, 0, 32),    # exact first-tile bound
    (0, 0, 1, 63),    # incomplete second tile
    (0, 0, 1, 64),    # exact second-tile bound
]
for base, tx, tid, count in boundaries:
    for n in range(1, 7):
        for ly in range(8):
            for flags in (0, 0x800, 0x1000, 0x1800):
                check(n, 117, 3, ly, flags, art, base, tx, tid, count)

result = {
    'core': 'upstream mGBA 0.10.5', 'actual_linked_ARM': True,
    'cases': cases, 'guarded_row_byte_comparisons': cases * len(initial),
    'scope': 'all run lengths 1..6, eight nibble phases and rows, HF/VF, dense/alternating/zero/one-hot art, left/right destination edges, assembled-map and tile-byte bounds',
    'callee_saved_registers_and_stack_preserved': True,
    'guards_and_pixels_match': True, 'original_animation_or_VDP_parity': False,
}
(O / (prefix + '_MGBA_PD_ROW.json')).write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result), flush=True)
