#!/usr/bin/env python3
"""VDP block-writer equivalence test: tiny SMS programs run on the GBA with two PSC8 builds (reference = per-byte OUTI/OTIR path, new = z80a_blk_c)
and the guest machine state is compared.
  blktest.py REF_ROM REF_ELF NEW_ROM NEW_ELF
Program A (no interrupts): block writes of many shapes (counts, VRAM addresses across the name-table / sprite-table / 16 KiB wrap, source pages,
  OTIR and "OUTI;JP NZ", CRAM fallback), after each one the V/H counters (T-state exact), the read buffer and a following data write are
  recorded to RAM.  Compared: guest RAM, VRAM, CRAM, and the GBA's VRAM/OAM/palette after the renderer ran.
Program B (frame interrupt on): endless 256-byte uploads; the guest state (registers, RAM, VRAM) is compared at several guest frames, which
  checks that the slice ends at the same instruction boundary as the per-byte path.
Exit code 0 when everything is identical.  Both builds must be made with PSC8_CPPFLAGS=-DPSC8_DEBUG_STOP."""
import sys, struct, subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[3]
sys.path.insert(0, str(R/'source/tools_host/cp08_mgba'))
sys.path.insert(0, str(R/'source/psc8/tools'))
from mgba2 import GBA
CART = 0x02028000
def sym(elf, name):
    for l in subprocess.run(['llvm-nm', elf], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[2] == name: return int(p[0], 16)
    raise SystemExit('no symbol ' + name)
class Asm:
    def __init__(s): s.b = bytearray()
    def raw(s, *x): s.b += bytes(x)
    def org(s, a): s.b += bytes(a - len(s.b))
    def pc(s): return len(s.b)
    def vreg(s, r, v): s.raw(0x3E, v, 0xD3, 0xBF, 0x3E, 0x80 | r, 0xD3, 0xBF)
    def vaddr(s, a, code): s.raw(0x3E, a & 255, 0xD3, 0xBF, 0x3E, ((a >> 8) & 0x3F) | (code << 6), 0xD3, 0xBF)
    def ld_hl(s, a): s.raw(0x21, a & 255, a >> 8)
    def ld_bc(s, b, c): s.raw(0x06, b & 255, 0x0E, c)
    def otir(s): s.raw(0xED, 0xB3)
    def outi_jp(s):
        a = s.pc(); s.raw(0xED, 0xA3, 0xC2, a & 255, a >> 8)
    def outi_seq(s, n): s.raw(*([0xED, 0xA3] * n))
    def outi_jp_far(s):                       # JP NZ goes to a NOP before the OUTI: not the fusable form
        a = s.pc(); s.raw(0x00, 0xED, 0xA3, 0xC2, a & 255, a >> 8)
    def sample(s, slot):                      # V counter, H counter, read buffer, then one more data write
        for i, p in enumerate((0x7E, 0x7F, 0xBE)): s.raw(0xDB, p, 0x32, (slot + i) & 255, 0xC1 + ((slot + i) >> 8))
        s.raw(0x3E, 0xEE, 0xD3, 0xBE)
def header(a):
    a.raw(0xF3, 0xED, 0x56, 0x31, 0xF0, 0xDF)
    for r, v in ((0, 0x04), (1, 0x40), (2, 0x0E), (5, 0x7E), (6, 0xFF), (7, 0x00), (10, 0xFF)): a.vreg(r, v)
def prog_a():
    a = Asm(); header(a)
    tests = [(0x1000, 0, 0x0000, 'otir'), (0x1000, 100, 0x380A, 'otir'), (0x1100, 37, 0x3EF0, 'outi'), (0x1200, 60, 0x3FE0, 'otir'),
             (0x13F0, 40, 0x0123, 'otir'), (0x1000, 3, 0x2000, 'otir'), (0x1000, 4, 0x2100, 'outi'), (0x1000, 5, 0x2200, 'otir'),
             (0x1500, 0, 0x3E80, 'outi'), (0x1600, 200, 0x3F80, 'outi'), (0x1000, 9, 0x3801, 'far'), (0x1700, 31, 0x3F3E, 'otir'),
             (0x1000, 64, 0x3F00, 'outi'), (0x1800, 130, 0x3B00, 'otir'), (0x1000, 17, 0x3FFF, 'outi'), (0x1000, 2, 0x3FFF, 'otir'),
             (0x1900, 20, 0x3EF8, 'seq20'), (0x1000, 8, 0x0FFE, 'seq3'), (0x13F8, 50, 0x3FF0, 'seq200'), (0x1000, 2, 0x2000, 'seq2'), (0x1A00, 120, 0x3700, 'seq7')]
    slot = 0
    for src, n, va, kind in tests:
        a.vaddr(va, 1); a.ld_hl(src)
        if kind.startswith('seq'): a.ld_bc(int(kind[3:]), 0xBE); a.outi_seq(n)
        else:
            a.ld_bc(n, 0xBE)
            {'otir': a.otir, 'outi': a.outi_jp, 'far': a.outi_jp_far}[kind]()
        a.sample(slot); slot += 3
    a.vaddr(0, 3); a.ld_hl(0x1000); a.ld_bc(32, 0xBE); a.otir(); a.sample(slot)      # CRAM (generic path)
    a.raw(0x18, 0xFE)                                                                  # JR $
    a.org(0x1000); a.raw(*(list(range(256)) * 16))
    return bytes(a.b).ljust(0x4000, b'\0')
def prog_b():
    a = Asm(); a.raw(0xC3, 0x00, 0x01); a.org(0x38); a.raw(0xF5, 0x3A, 0x00, 0xC2, 0x3C, 0x32, 0x00, 0xC2, 0xDB, 0xBF, 0xF1, 0xFB, 0xED, 0x4D)   # ISR: counter++, read status, RETI
    a.org(0x66); a.raw(0xED, 0x45)
    a.org(0x100)
    a.raw(0xF3, 0xED, 0x56, 0x31, 0xF0, 0xDF)
    for r, v in ((0, 0x04), (1, 0x60), (2, 0x0E), (5, 0x7E), (6, 0xFF), (7, 0x00), (10, 0xFF)): a.vreg(r, v)
    a.raw(0xFB)
    top = a.pc()
    a.raw(0x0E, 0xBE)
    a.vaddr(0x3C00, 1); a.ld_hl(0x1000); a.raw(0x06, 0x00); a.otir()
    a.vaddr(0x3E00, 1); a.ld_hl(0x1100); a.raw(0x06, 0x00); a.outi_jp()
    a.vaddr(0x0100, 1); a.ld_hl(0x13A0); a.raw(0x06, 0x60); a.otir()
    a.vaddr(0x3F00, 1); a.ld_hl(0x1200); a.raw(0x06, 0x50); a.outi_jp()
    a.vaddr(0x2000, 1); a.ld_hl(0x1300); a.raw(0x06, 0x40); a.outi_seq(100)
    a.raw(0xC3, top & 255, top >> 8)
    a.org(0x1000); a.raw(*(list(range(256)) * 16))
    return bytes(a.b).ljust(0x4000, b'\0')
def boot(rom, prog):
    g = GBA(rom, entry=0x086ca500)
    g.put(CART, bytes(prog) + bytes(0x8000 - len(prog))); g.put(0x0203FF00, struct.pack('<IIII', CART, 0x8000, 0, 0))
    return g
def dump_state(g, elf):
    ctx = sym(elf, 'z80a_ctx'); ram = sym(elf, 'ram'); vram = sym(elf, 'vram'); cram = sym(elf, 'cram')
    regs = struct.unpack('<7I', g.mem(ctx + 0x164, 28))                       # A F BC DE HL PC SP
    return dict(regs=regs[:2] + tuple(r & 0xffff for r in regs[2:]), ram=g.mem(ram, 8192), vram=g.mem(vram, 16384), cram=g.mem(cram, 64))
def run_a(rom, elf):
    g = boot(rom, prog_a()); g.run(60)
    st = dump_state(g, elf); st['gba_vram'] = g.mem(0x06000000, 0x18000); st['gba_oam'] = g.mem(0x07000000, 0x400); st['gba_pal'] = g.mem(0x05000000, 0x400)
    return st
def diff(name, x, y):
    bad = 0
    for k in x:
        if x[k] != y[k]:
            if isinstance(x[k], (bytes, bytearray)):
                idx = [i for i in range(len(x[k])) if x[k][i] != y[k][i]]
                print(f'  {name}: {k} differs at {len(idx)} bytes, first {idx[:6]}');
            else: print(f'  {name}: {k} differs: {x[k]} vs {y[k]}')
            bad += 1
    return bad
def run_b(rom, elf, frames):
    """one run per frame number: the PSC8_DEBUG_STOP build freezes at the end of guest frame f, so the state is the one at a frame boundary"""
    out = {}
    for f in frames:
        g = boot(rom, prog_b()); g.wr32(0x0203E0F0, f)
        for _ in range(4 * f + 60):
            g.run(1)
            if g.rd32(0x0203E004) >= f: break
        g.run(3)
        out[f] = dump_state(g, elf) if g.rd32(0x0203E004) == f else None
    return out
if __name__ == '__main__':
    rr, re_, nr, ne = sys.argv[1:5]
    bad = 0
    sa, sb = run_a(rr, re_), run_a(nr, ne)
    bad += diff('A', sa, sb)
    ram = sa['ram']; rec = [(ram[0x100 + 3 * i], ram[0x101 + 3 * i], ram[0x102 + 3 * i]) for i in range(22)]
    print('A: recorded V/H/readbuf of the reference run (first 6):', rec[:6])
    print('A:', 'identical' if not bad else 'MISMATCH')
    frames = [2, 3, 5, 9, 14, 20, 33]
    ra, rb = run_b(rr, re_, frames), run_b(nr, ne, frames)
    for f in frames:
        if ra[f] is None or rb[f] is None: print(f'B frame {f}: could not stop exactly on it'); continue
        b = diff(f'B@{f}', ra[f], rb[f]); bad += b
        print(f'B frame {f}:', 'identical' if not b else 'MISMATCH', 'pc', hex(ra[f]['regs'][5]), 'hl', hex(ra[f]['regs'][4]), 'bc', hex(ra[f]['regs'][2]))
    sys.exit(1 if bad else 0)
