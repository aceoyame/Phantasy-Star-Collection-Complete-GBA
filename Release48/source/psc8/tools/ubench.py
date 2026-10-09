#!/usr/bin/env python3
"""Z80-core micro benchmarks on the GBA: tiny SMS cartridges (hand-assembled loops) run through PSC8; reports how many guest T-states
run per display frame (1.0 = real time) when the loop keeps the CPU 100% busy, plus the implied GBA cycles per iteration.
  ubench.py ROM [case ...]      (ROM = a collection ROM with PSC8 installed, as tools/inject.py makes)"""
import sys, struct, json
sys.path.insert(0, '/home/claude/work/handoff/CLAUDE_R48_CP07_HANDOFF/Release48/source/tools_host/cp08_mgba')
from mgba2 import GBA
CART = 0x02011000          # free EWRAM above PSC8's bss (below the heartbeat)
def prog(*parts):
    b = bytearray()
    for p in parts: b += bytes(p)
    return b
# preamble: DI, IM 1, LD SP,0xDFF0, VDP address 0 / write code
PRE = [0xF3, 0xED, 0x56, 0x31, 0xF0, 0xDF, 0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0x40, 0xD3, 0xBF]
def case(name):
    L0 = len(PRE)
    if name == 'outi_jpnz':     # LD HL,0x1000; LD B,0; LD C,0xBE; l: OUTI; JP NZ,l; JR top   -> 256 iterations of OUTI(16)+JP(10)
        code = [0x21, 0x00, 0x10, 0x06, 0x00, 0x0E, 0xBE, 0xED, 0xA3, 0xC2, L0 + 7, 0x00, 0x18, 0xF3]; titer = 26
    elif name == 'otir':        # OTIR x256
        code = [0x21, 0x00, 0x10, 0x06, 0x00, 0x0E, 0xBE, 0xED, 0xB3, 0x18, 0xF6]; titer = 21
    elif name == 'out_bf_loop': # LD B,0; l: OUT (BE),A; DJNZ l   -> 11+13
        code = [0x06, 0x00, 0xD3, 0xBE, 0x10, 0xFC, 0x18, 0xF8]; titer = 24
    elif name == 'ldir':        # LD HL,0x1000; LD DE,0xC000; LD BC,0x400; LDIR; JR top
        code = [0x21, 0x00, 0x10, 0x11, 0x00, 0xC0, 0x01, 0x00, 0x04, 0xED, 0xB0, 0x18, 0xF3]; titer = 21
    elif name == 'ldi_jppe':    # LD HL..; LD BC,0x400; l: LDI; JP PE,l; JR top
        code = [0x21, 0x00, 0x10, 0x11, 0x00, 0xC0, 0x01, 0x00, 0x04, 0xED, 0xA0, 0xEA, L0 + 9, 0x00, 0x18, 0xF0]; titer = 26
    elif name == 'nop':
        code = [0x00] * 8 + [0x18, 0xF6]; titer = 8 * 4 / 8     # per NOP
    elif name == 'ld_a_b':
        code = [0x78] * 8 + [0x18, 0xF6]; titer = 4
    elif name == 'add_a_b':
        code = [0x80] * 8 + [0x18, 0xF6]; titer = 4
    elif name == 'ld_a_hl':     # LD A,(HL) with HL in RAM
        code = [0x21, 0x00, 0xC1] + [0x7E] * 8 + [0x18, 0xF3]; titer = 7
    elif name == 'ld_hl_a':
        code = [0x21, 0x00, 0xC1] + [0x77] * 8 + [0x18, 0xF3]; titer = 7
    elif name == 'jr_loop':     # tight JR NZ loop: LD B,0 ; l: DEC B ; JR NZ,l (counts 256)
        code = [0x06, 0x00, 0x05, 0x20, 0xFD, 0x18, 0xF9]; titer = 16 + 0
    elif name == 'call_ret':
        code = [0xCD, L0 + 4, 0x00, 0x18, 0xFB, 0xC9]; titer = 27
    elif name == 'push_pop':
        code = [0xC5, 0xC1, 0xC5, 0xC1, 0x18, 0xFA]; titer = 21
    elif name == 'in_port':     # IN A,(0xDC)
        code = [0xDB, 0xDC, 0x18, 0xFC]; titer = 11
    elif name == 'in_vcnt':
        code = [0xDB, 0x7E, 0x18, 0xFC]; titer = 11
    else: raise SystemExit('unknown case ' + name)
    img = bytearray(prog(PRE, code)); img += bytes(0x1000 - len(img))
    img += bytes(range(256)) * 16; img += bytes(0x4000 - len(img))     # 0x1000.. data
    return img, titer
CASES = ['outi_jpnz', 'otir', 'out_bf_loop', 'ldir', 'ldi_jppe', 'nop', 'ld_a_b', 'add_a_b', 'ld_a_hl', 'ld_hl_a', 'jr_loop', 'call_ret', 'push_pop', 'in_port', 'in_vcnt']
def run(rom, name, frames=240):
    img, titer = case(name)
    g = GBA(rom, entry=0x086ca500)
    g.put(CART, bytes(img) + bytes(0x8000 - len(img)))
    g.put(0x0203FF00, struct.pack('<IIII', CART, 0x8000, 0, 0))
    g.run(30)
    v0 = struct.unpack('<14I', g.mem(0x0203E000, 56)); z0 = v0[7]; f0 = v0[1]
    g.run(frames)
    v1 = struct.unpack('<14I', g.mem(0x0203E000, 56))
    dz = (v1[7] - z0) & 0xffffffff; df = v1[1] - f0
    speed = df / frames
    # implied GBA cycles per iteration (all of the display frame's 280896 cycles go to the guest and the fixed per-frame work)
    its = dz / titer
    return dict(case=name, speed=round(speed, 3), guestT_per_frame=round(dz / frames), cycles_per_iter=round(280896 * frames / its, 1))
if __name__ == '__main__':
    rom = sys.argv[1]
    for c in sys.argv[2:] or CASES:
        print(json.dumps(run(rom, c)), flush=True)
