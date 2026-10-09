#!/usr/bin/env python3
"""Generates z80a.s: PSC8's ARM7TDMI assembly Z80 core (threaded dispatch, pinned registers).

Semantics follow z80f.h (the verified C core: ZEXDOC clean, lockstep-identical to the CP03 core).

ARM register map while running guest code
  r0-r2  scratch (r2 is also the link register of the internal write helper)
  r3     main opcode jump table
  r4     context block (z80a_ctx): rmap[68] at +0, scalars at +0x120, flag tables at +0x200
  r5     Z80 PC (may exceed 0xFFFF by a few bytes between jumps; rmap[64..67] mirror pages 0..3)
  r6     cycle budget, counting down; the slice ends after the instruction that makes it <= 0
  r7     A        r8  F
  r9     HL       r10 BC      r11 DE      r12 SP
  lr     flag tables: SZP +0, SZ +256, INC +512, DEC +768
"""
import sys
TEST = '--test' in sys.argv
if TEST: sys.argv.remove('--test')

import re, os
def _tab(name):
    src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'z80f.h')).read()
    m = re.search(r'z80f_%s\[256\] Z80F_CDATA = \{([^}]*)\}' % name, src)
    return [int(x) for x in m.group(1).split(',')]
CYC00, CYCED, CYCDD = _tab('cyc_00'), _tab('cyc_ed'), _tab('cyc_ddfd')
assert len(CYC00) == len(CYCED) == len(CYCDD) == 256

# context offsets (mirrored in z80a.h, generated below)
O = dict(IX=0x120, IY=0x124, SPSAVE=0x128, AF2=0x12C, BC2=0x130, DE2=0x134, HL2=0x138, HLSAVE=0x13C, XYOFF=0x140,
         LINK=0x144, MAINTAB=0x148, SAVEDBUDGET=0x14C, I=0x150, R=0x151, IM=0x152, IFF1=0x153, IFF2=0x154, HALTED=0x155,
         INTP=0x156, INTDATA=0x157, SRAMEN=0x158, IRQCHK=0x159, RHI=0x15A, BUDGET=0x15C, R6NOW=0x160, A=0x164, F=0x168,
         BC=0x16C, DE=0x170, HL=0x174, PC=0x178, SP=0x17C, TMP=0x180, TMP2=0x184, DAATAB=0x188, LINK2=0x18C, VCOUNT=0x190, VADDR=0x194, VCODE=0x198, CMDFIRST=0x199, READBUF=0x19A, SPRDIRTY=0x19B, VRAMPTR=0x19C, TILEBITS=0x1A0, MAPBITS=0x1A4, NB=0x1A8, SB=0x1AC, CMDLO=0x1B0, VOFF=0x1B4, VLINE0=0x1B8, BREAK=0x1BC, INCNT=0x1C0, GAIDEN=0x1C4)
TAB = 0x200
CTX_SIZE = 0x800
FASTIN = 0x600   # 256 bytes: nonzero = port value is constant for the frame (value at VALIN)
VALIN = 0x700

CF, NF, PF, XF, HF, YF, ZF, SF = 1, 2, 4, 8, 16, 32, 64, 128

out = []
def a(*lines):
    for l in lines:
        out.append(('    ' + l) if not l.endswith(':') and not l.startswith('.') else l)

lbl_n = [0]
def L():
    lbl_n[0] += 1
    return f'.Lz{lbl_n[0]}'

# ---------------------------------------------------------------- primitives
def fetch(dst='r0', t='r1'):
    return [f'mov {t}, r5, lsr #10', f'ldr {t}, [r4, {t}, lsl #2]', f'ldrb {dst}, [{t}, r5]', 'add r5, r5, #1']
def fetch16(dst='r0'):
    # dst = lo | hi<<8   (uses r0,r1,r2)
    return fetch('r0', 'r1') + fetch('r2', 'r1') + [f'orr {dst}, r0, r2, lsl #8']
def rd(dst, addr, t='r2'):
    return [f'mov {t}, {addr}, lsr #10', f'ldr {t}, [r4, {t}, lsl #2]', f'ldrb {dst}, [{t}, {addr}]']
def wr():
    """write r1 to address r0 (both < 0x10000, r1 < 256); clobbers r0,r2"""
    return ['mov r2, pc', 'b z80a_wr8']
def mask16(r, src=None):
    src = src or r
    return [f'mov {r}, {src}, lsl #16', f'mov {r}, {r}, lsr #16']
INLINE_NEXT = {f'op_{x:02X}' for x in (0xCD, 0xDB, 0xFE, 0x2F, 0xC9, 0xE6, 0x20, 0xF6, 0x38, 0x0F, 0x17, 0x28, 0x07, 0x10, 0x3A, 0xCA, 0x30, 0xC4, 0x21, 0xDA, 0xC3, 0x3E, 0xF5, 0xF1, 0xC5, 0xC1)}   # handler names that get a private fetch/dispatch tail (speed); others share one (size)
# Additional measured hot tails: unary multiply, VDP output, and common indexed loads/adds.
INLINE_NEXT |= {'op_87','op_D3','xy_7E','xy_19','xy_23'}
CUR = ['']
def next_(cyc):
    if CUR[0] not in INLINE_NEXT:
        return ([f'subs r6, r6, #{cyc}'] if cyc else ['cmp r6, #0']) + ['bgt z80a_dispatch', 'b z80a_exit']
    l = []
    if cyc:
        l.append(f'subs r6, r6, #{cyc}')
    else:
        l.append('cmp r6, #0')
    l += ['ble z80a_exit'] + fetch('r0', 'r1') + ['ldr pc, [r3, r0, lsl #2]']
    return l
def xynext(cyc):
    return [f'ldr r0, [r4, #{O["XYOFF"]}]', 'str r9, [r4, r0]', f'ldr r9, [r4, #{O["HLSAVE"]}]'] + next_(cyc)

# 8-bit register access: index 0..7 = B C D E H L (HL) A
def get8(i, dst):
    return {0: [f'mov {dst}, r10, lsr #8'], 1: [f'and {dst}, r10, #255'], 2: [f'mov {dst}, r11, lsr #8'],
            3: [f'and {dst}, r11, #255'], 4: [f'mov {dst}, r9, lsr #8'], 5: [f'and {dst}, r9, #255'],
            7: [f'mov {dst}, r7'] if dst != 'r7' else []}[i]
def set8(i, v):
    return {0: ['and r10, r10, #255', f'orr r10, r10, {v}, lsl #8'], 1: ['bic r10, r10, #255', f'orr r10, r10, {v}'],
            2: ['and r11, r11, #255', f'orr r11, r11, {v}, lsl #8'], 3: ['bic r11, r11, #255', f'orr r11, r11, {v}'],
            4: ['and r9, r9, #255', f'orr r9, r9, {v}, lsl #8'], 5: ['bic r9, r9, #255', f'orr r9, r9, {v}'],
            7: [f'mov r7, {v}'] if v != 'r7' else []}[i]

# flags helpers (v in r0)
def alu(k):
    """A op r0 -> A, F.  Uses r0,r1,r2 only."""
    if k in ('ADD', 'ADC', 'SUB', 'SBC', 'CP'):
        sub = k in ('SUB', 'SBC', 'CP')
        op = 'sub' if sub else 'add'
        l = []
        if k in ('ADC', 'SBC'):
            l += ['and r2, r8, #1', f'{op} r1, r7, r0', f'{op} r1, r1, r2']
        else:
            l += [f'{op} r1, r7, r0']
        if k == 'CP':
            l += ['and r8, r0, #0x28']                 # X/Y from the operand
        l += ['eor r2, r7, r0', 'eor r2, r2, r1', 'and r2, r2, #0x10']
        l += (['orr r8, r8, r2'] if k == 'CP' else ['mov r8, r2'])
        l += ['eor r2, r7, r0']
        if not sub:
            l += ['mvn r2, r2']
        l += ['eor r0, r7, r1', 'and r2, r2, r0', 'and r2, r2, #0x80', 'orr r8, r8, r2, lsr #5']   # V
        l += ['tst r1, #0x100', 'orrne r8, r8, #1']                                                 # C
        if sub:
            l += ['orr r8, r8, #2']
        l += ['and r1, r1, #255', 'add r2, lr, #256', 'ldrb r2, [r2, r1]']                          # SZ
        if k == 'CP':
            l += ['bic r2, r2, #0x28', 'orr r8, r8, r2']
        else:
            l += ['orr r8, r8, r2', 'mov r7, r1']
        return l, False
    if k == 'AND': return ['and r7, r7, r0', 'ldrb r8, [lr, r7]', 'orr r8, r8, #0x10'], False
    if k == 'XOR': return ['eor r7, r7, r0', 'ldrb r8, [lr, r7]'], False
    if k == 'OR':  return ['orr r7, r7, r0', 'ldrb r8, [lr, r7]'], False

ALU = ['ADD', 'ADC', 'SUB', 'SBC', 'AND', 'XOR', 'OR', 'CP']
SPS = f'str r12, [r4, #{O["SPSAVE"]}]'
SPL = f'ldr r12, [r4, #{O["SPSAVE"]}]'

def alu_v(k):
    """ALU A,r0 with SP preserved"""
    body, uses12 = alu(k)
    return ([SPS] + body + [SPL]) if uses12 else body

def inc8(dst):  # dst value register (0..255) -> incremented, flags
    return [f'add {dst}, {dst}, #1', f'and {dst}, {dst}, #255', 'and r8, r8, #1', 'add r2, lr, #512', f'ldrb r2, [r2, {dst}]', 'orr r8, r8, r2']
def dec8(dst):
    return [f'sub {dst}, {dst}, #1', f'and {dst}, {dst}, #255', 'and r8, r8, #1', 'add r2, lr, #768', f'ldrb r2, [r2, {dst}]', 'orr r8, r8, r2']

def push(reg):  # reg holds 16-bit value (not r0..r2)
    return ['sub r12, r12, #2'] + mask16('r12') + ['add r0, r12, #1'] + mask16('r0') + [f'mov r1, {reg}, lsr #8'] + wr() + \
           ['mov r0, r12', f'and r1, {reg}, #255'] + wr()
def pop(dst):   # dst 16-bit (not r0..r2)
    return rd('r0', 'r12', 'r2') + ['add r12, r12, #1'] + rd('r1', 'r12', 'r2') + ['add r12, r12, #1', 'bic r12, r12, #0x10000',
            f'orr {dst}, r0, r1, lsl #8']

def add16(dstreg, vreg):
    """dst = dst + v (16 bit); F = (F&(S|Z|P)) | C | H | Y/X from high byte. clobbers r0..r2"""
    return [f'add r0, {dstreg}, {vreg}', f'eor r1, {dstreg}, {vreg}', 'eor r1, r1, r0', 'and r8, r8, #0xC4',
            'and r1, r1, #0x1000', 'orr r8, r8, r1, lsr #8', 'tst r0, #0x10000', 'orrne r8, r8, #1',
            'and r1, r0, #0x2800', 'orr r8, r8, r1, lsr #8'] + mask16(dstreg, 'r0')

# ---------------------------------------------------------------- handlers
handlers = {}      # name -> list of lines (without label)
main_tab = ['op_ill'] * 256
xy_tab = ['xy_plain'] * 256
ed_tab = ['ed_nop'] * 256

def H(name, lines):
    assert name not in handlers, name
    handlers[name] = lines

REGN = ['B', 'C', 'D', 'E', 'H', 'L', '(HL)', 'A']

def gen_main():
    for op in range(256):
        nm = f'op_{op:02X}'; CUR[0] = nm
        c = CYC00[op]
        l = None
        x, y, z = op >> 6, (op >> 3) & 7, op & 7
        if op == 0x00: l = next_(4)
        elif op in (0x01, 0x11, 0x21, 0x31):
            dst = ['r10', 'r11', 'r9', 'r12'][op >> 4]
            l = fetch16(dst) + next_(c)
        elif op in (0x03, 0x13, 0x23, 0x33, 0x0B, 0x1B, 0x2B, 0x3B):
            dst = ['r10', 'r11', 'r9', 'r12'][(op >> 4) & 3]
            l = [f'{"add" if (op & 8) == 0 else "sub"} {dst}, {dst}, #1'] + mask16(dst) + next_(c)
        elif op in (0x09, 0x19, 0x29, 0x39):
            v = ['r10', 'r11', 'r9', 'r12'][op >> 4]
            l = add16('r9', v) + next_(c)
        elif op == 0x02: l = ['mov r0, r10', 'mov r1, r7'] + wr() + next_(c)
        elif op == 0x12: l = ['mov r0, r11', 'mov r1, r7'] + wr() + next_(c)
        elif op == 0x0A: l = rd('r7', 'r10') + next_(c)
        elif op == 0x1A: l = rd('r7', 'r11') + next_(c)
        elif op == 0x22: l = fetch16('r0') + ['and r1, r9, #255', f'str r0, [r4, #{O["TMP"]}]'] + wr() + \
                             [f'ldr r0, [r4, #{O["TMP"]}]', 'add r0, r0, #1'] + mask16('r0') + ['mov r1, r9, lsr #8'] + wr() + next_(c)
        elif op == 0x2A: l = fetch16('r1') + ['mov r0, r1'] + rd('r9', 'r0') + ['add r0, r0, #1'] + mask16('r0') + rd('r1', 'r0') + \
                             ['orr r9, r9, r1, lsl #8'] + next_(c)
        elif op == 0x32: l = fetch16('r0') + ['mov r1, r7'] + wr() + next_(c)
        elif op == 0x3A: l = fetch16('r0') + rd('r7', 'r0') + next_(c)
        elif x == 0 and z in (4, 5) and y != 6:
            l = get8(y, 'r1') + (inc8('r1') if z == 4 else dec8('r1')) + set8(y, 'r1') + next_(c)
        elif op in (0x34, 0x35):
            l = rd('r1', 'r9') + (inc8('r1') if op == 0x34 else dec8('r1')) + ['mov r0, r9'] + wr() + next_(c)
        elif x == 0 and z == 6 and y != 6:
            l = fetch('r1', 'r2') + set8(y, 'r1') + next_(c)
        elif op == 0x36: l = fetch('r1', 'r2') + ['mov r0, r9'] + wr() + next_(c)
        elif op == 0x07:   # RLCA
            l = ['mov r7, r7, lsl #1', 'orr r7, r7, r7, lsr #8', 'and r7, r7, #255', 'and r8, r8, #0xC4', 'and r0, r7, #0x29', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x0F:   # RRCA
            l = ['and r8, r8, #0xC4', 'and r0, r7, #1', 'orr r8, r8, r0', 'orr r7, r7, r7, lsl #8', 'mov r7, r7, lsr #1', 'and r7, r7, #255',
                 'and r0, r7, #0x28', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x17:   # RLA
            l = ['and r0, r8, #1', 'orr r7, r0, r7, lsl #1', 'and r8, r8, #0xC4', 'orr r8, r8, r7, lsr #8', 'and r7, r7, #255',
                 'and r0, r7, #0x28', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x1F:   # RRA
            l = ['and r0, r8, #1', 'and r8, r8, #0xC4', 'and r1, r7, #1', 'orr r8, r8, r1', 'orr r7, r7, r0, lsl #8', 'mov r7, r7, lsr #1',
                 'and r0, r7, #0x28', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x08:
            l = [f'ldr r0, [r4, #{O["AF2"]}]', 'orr r1, r8, r7, lsl #8', f'str r1, [r4, #{O["AF2"]}]', 'mov r7, r0, lsr #8', 'and r8, r0, #255'] + next_(c)
        elif op == 0x10:   # DJNZ
            k = L()
            l = fetch('r0', 'r1') + ['sub r10, r10, #0x100', 'mov r10, r10, lsl #16', 'mov r10, r10, lsr #16', 'tst r10, #0xFF00', f'beq {k}',
                 'mov r0, r0, lsl #24', 'add r5, r5, r0, asr #24'] + mask16('r5') + ['sub r6, r6, #5', f'{k}:'] + next_(c)
        elif op == 0x18:
            l = fetch('r0', 'r1') + ['mov r0, r0, lsl #24', 'add r5, r5, r0, asr #24'] + mask16('r5') + next_(c)
        elif op in (0x20, 0x28, 0x30, 0x38):
            cond = {0x20: ('tst r8, #0x40', 'ne'), 0x28: ('tst r8, #0x40', 'eq'), 0x30: ('tst r8, #1', 'ne'), 0x38: ('tst r8, #1', 'eq')}[op]
            k = L()
            idle = []
            if op in (0x20, 0x28):
                sk = L()
                idle = ['cmp r0, #0xF9000000', f'blo {sk}', 'cmp r0, #0xFE000000', f'bhs {sk}', 'bl z80a_idle', f'add lr, r4, #{TAB}', f'{sk}:']
            l = fetch('r0', 'r1') + [cond[0], f'b{cond[1]} {k}', 'mov r0, r0, lsl #24', 'add r5, r5, r0, asr #24'] + mask16('r5') + \
                idle + ['sub r6, r6, #5', f'{k}:'] + next_(c)
        elif op == 0x27:
            l = [f'ldr r1, [r4, #{O["DAATAB"]}]', 'and r0, r8, #0x13', 'and r2, r0, #3', 'orr r2, r2, r0, lsr #2', 'orr r0, r7, r2, lsl #8',
                 'mov r0, r0, lsl #1', 'ldrh r0, [r1, r0]', 'mov r7, r0, lsr #8', 'and r8, r0, #255'] + next_(c)
        elif op == 0x2F:
            l = ['eor r7, r7, #255', 'and r8, r8, #0xC5', 'orr r8, r8, #0x12', 'and r0, r7, #0x28', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x37:
            l = ['and r8, r8, #0xC4', 'orr r8, r8, #1', 'and r0, r7, #0x28', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x3F:
            l = ['and r0, r8, #1', 'and r8, r8, #0xC5', 'orr r8, r8, r0, lsl #4', 'eor r8, r8, #1', 'and r0, r7, #0x28', 'orr r8, r8, r0'] + next_(c)
        elif op == 0x76:   # HALT
            l = ['mov r0, #1', f'strb r0, [r4, #{O["HALTED"]}]', 'sub r5, r5, #1', 'sub r6, r6, #4',
                 'cmp r6, #0', 'ble z80a_exit', 'add r0, r6, #3', 'bic r0, r0, #3', 'sub r6, r6, r0', 'b z80a_exit']
        elif x == 1:
            d, s = y, z
            if d == 6:
                l = get8(s, 'r1') + ['mov r0, r9'] + wr() + next_(c)
            elif s == 6:
                l = rd('r1', 'r9') + set8(d, 'r1') + next_(c)
            elif d == s:
                l = next_(c)
            else:
                l = get8(s, 'r1') + set8(d, 'r1') + next_(c)
        elif x == 2:
            src = rd('r0', 'r9', 'r2') if z == 6 else get8(z, 'r0')
            l = src + alu_v(ALU[y]) + next_(c)
        elif x == 3 and z == 6:
            l = fetch('r0', 'r1') + alu_v(ALU[y]) + next_(c)
        elif x == 3 and z == 0:    # RET cc
            l = cc_ret(y, c)
        elif x == 3 and z == 2:    # JP cc
            t, f = cc_test(y)
            k = L()
            l = fetch16('r0') + [t, f'b{f} {k}', 'mov r5, r0', f'{k}:'] + next_(c)
        elif x == 3 and z == 4:    # CALL cc
            t, f = cc_test(y)
            k = L()
            l = fetch16('r0') + [t, f'b{f} {k}', f'str r0, [r4, #{O["TMP2"]}]'] + mask16('r1', 'r5') + [f'str r1, [r4, #{O["TMP"]}]'] + \
                push_tmp() + [f'ldr r5, [r4, #{O["TMP2"]}]', 'sub r6, r6, #7', f'{k}:'] + next_(c)
        elif x == 3 and z == 7:    # RST
            l = mask16('r1', 'r5') + [f'str r1, [r4, #{O["TMP"]}]'] + push_tmp() + [f'mov r5, #{8 * y}'] + next_(c)
        elif op in (0xC1, 0xD1, 0xE1):
            dst = {0xC1: 'r10', 0xD1: 'r11', 0xE1: 'r9'}[op]
            l = pop(dst) + next_(c)
        elif op == 0xF1:
            l = pop('r1') + ['mov r7, r1, lsr #8', 'and r8, r1, #255'] + next_(c)
        elif op in (0xC5, 0xD5, 0xE5):
            src = {0xC5: 'r10', 0xD5: 'r11', 0xE5: 'r9'}[op]
            l = [f'str {src}, [r4, #{O["TMP"]}]'] + push_tmp() + next_(c)
        elif op == 0xF5:
            l = ['orr r1, r8, r7, lsl #8', f'str r1, [r4, #{O["TMP"]}]'] + push_tmp() + next_(c)
        elif op == 0xC3: l = fetch16('r5') + next_(c)
        elif op == 0xC9: l = pop('r5') + next_(c)
        elif op == 0xCD:
            l = fetch16('r0') + [f'str r0, [r4, #{O["TMP2"]}]'] + mask16('r1', 'r5') + [f'str r1, [r4, #{O["TMP"]}]'] + push_tmp() + \
                [f'ldr r5, [r4, #{O["TMP2"]}]'] + next_(c)
        elif op == 0xD3:
            l = fetch('r0', 'r1') + ['mov r1, r7', 'bl z80a_out_call'] + next_(c)
        elif op == 0xDB:
            l = fetch('r0', 'r1') + ['bl z80a_in_a', 'mov r7, r0'] + next_(c)
        elif op == 0xD9:   # EXX
            l = [f'ldr r0, [r4, #{O["BC2"]}]', f'str r10, [r4, #{O["BC2"]}]', 'mov r10, r0',
                 f'ldr r0, [r4, #{O["DE2"]}]', f'str r11, [r4, #{O["DE2"]}]', 'mov r11, r0',
                 f'ldr r0, [r4, #{O["HL2"]}]', f'str r9, [r4, #{O["HL2"]}]', 'mov r9, r0'] + next_(c)
        elif op == 0xEB: l = ['mov r0, r9', 'mov r9, r11', 'mov r11, r0'] + next_(c)
        elif op == 0xE3:   # EX (SP),HL
            l = ex_sp('r9') + next_(c)
        elif op == 0xE9: l = ['mov r5, r9'] + next_(c)
        elif op == 0xF9: l = ['mov r12, r9'] + next_(c)
        elif op == 0xF3:
            l = ['mov r0, #0', f'strb r0, [r4, #{O["IFF1"]}]', f'strb r0, [r4, #{O["IFF2"]}]'] + next_(c)
        elif op == 0xFB:
            l = ['mov r0, #1', f'strb r0, [r4, #{O["IFF1"]}]', f'strb r0, [r4, #{O["IFF2"]}]', f'subs r6, r6, #{c}', 'b z80a_arm_irq']
        elif op == 0xCB: l = ['b z80a_cb']
        elif op == 0xED: l = ['b z80a_ed']
        elif op == 0xDD: l = [f'mov r1, #{O["IX"]}', 'b z80a_xy']
        elif op == 0xFD: l = [f'mov r1, #{O["IY"]}', 'b z80a_xy']
        assert l is not None, hex(op)
        H(nm, l)
        main_tab[op] = nm

def push_tmp():
    """push the 16-bit value stored in ctx TMP"""
    return ['sub r12, r12, #2'] + mask16('r12') + ['add r0, r12, #1'] + mask16('r0') + [f'ldr r1, [r4, #{O["TMP"]}]', 'mov r1, r1, lsr #8'] + wr() + \
           ['mov r0, r12', f'ldr r1, [r4, #{O["TMP"]}]', 'and r1, r1, #255'] + wr()

def cc_test(y):
    # returns (test instruction, branch-condition-to-SKIP)
    return {0: ('tst r8, #0x40', 'ne'), 1: ('tst r8, #0x40', 'eq'), 2: ('tst r8, #1', 'ne'), 3: ('tst r8, #1', 'eq'),
            4: ('tst r8, #4', 'ne'), 5: ('tst r8, #4', 'eq'), 6: ('tst r8, #0x80', 'ne'), 7: ('tst r8, #0x80', 'eq')}[y]

def cc_ret(y, c):
    t, f = cc_test(y)
    k = L()
    return [t, f'b{f} {k}'] + pop('r5') + ['sub r6, r6, #6', f'{k}:'] + next_(c)

def ex_sp(reg):
    return rd('r0', 'r12', 'r2') + ['add r1, r12, #1'] + mask16('r1') + rd('r1', 'r1', 'r2') + ['orr r0, r0, r1, lsl #8', f'str r0, [r4, #{O["TMP"]}]',
            'mov r0, r12', f'and r1, {reg}, #255'] + wr() + ['add r0, r12, #1'] + mask16('r0') + [f'mov r1, {reg}, lsr #8'] + wr() + \
           [f'ldr {reg}, [r4, #{O["TMP"]}]']

# ---------------------------------------------------------------- DD/FD (r9 holds IX/IY while an XY handler runs)
def ea_xy():
    return fetch('r0', 'r1') + ['mov r0, r0, lsl #24', 'add r0, r9, r0, asr #24'] + mask16('r0')

def gen_xy():
    HLOPS = set()
    for op in range(256):
        x, y, z = op >> 6, (op >> 3) & 7, op & 7
        nm = f'xy_{op:02X}'; CUR[0] = nm
        c = CYCDD[op]
        l = None
        # register-only ops that touch H/L/HL become IXH/IXL/IX: same code with r9 = IX
        if op in (0x09, 0x19, 0x39):
            v = {0x09: 'r10', 0x19: 'r11', 0x39: 'r12'}[op]
            l = add16('r9', v) + xynext(c)
        elif op == 0x29: l = add16('r9', 'r9') + xynext(c)
        elif op == 0x21: l = fetch16('r9') + xynext(c)
        elif op == 0x22: l = fetch16('r0') + ['and r1, r9, #255', f'str r0, [r4, #{O["TMP"]}]'] + wr() + \
                             [f'ldr r0, [r4, #{O["TMP"]}]', 'add r0, r0, #1'] + mask16('r0') + ['mov r1, r9, lsr #8'] + wr() + xynext(c)
        elif op == 0x2A: l = fetch16('r1') + ['mov r0, r1'] + rd('r9', 'r0') + ['add r0, r0, #1'] + mask16('r0') + rd('r1', 'r0') + ['orr r9, r9, r1, lsl #8'] + xynext(c)
        elif op in (0x23, 0x2B): l = [f'{"add" if op == 0x23 else "sub"} r9, r9, #1'] + mask16('r9') + xynext(c)
        elif op in (0x24, 0x25, 0x2C, 0x2D):
            i = 4 if op < 0x28 else 5
            l = get8(i, 'r1') + (inc8('r1') if op & 1 == 0 else dec8('r1')) + set8(i, 'r1') + xynext(c)
        elif op in (0x26, 0x2E):
            l = fetch('r1', 'r2') + set8(4 if op == 0x26 else 5, 'r1') + xynext(c)
        elif op in (0x34, 0x35):
            l = ea_xy() + [f'str r0, [r4, #{O["TMP"]}]'] + rd('r1', 'r0') + (inc8('r1') if op == 0x34 else dec8('r1')) + \
                [f'ldr r0, [r4, #{O["TMP"]}]'] + wr() + xynext(c)
        elif op == 0x36:
            l = ea_xy() + [f'str r0, [r4, #{O["TMP"]}]'] + fetch('r1', 'r2') + [f'ldr r0, [r4, #{O["TMP"]}]'] + wr() + xynext(c)
        elif x == 1 and op != 0x76:
            d, s = y, z
            if d == 6:     # LD (XY+d),r  (r is the real register; H/L from HLSAVE)
                if s in (4, 5):
                    src = [f'ldr r1, [r4, #{O["HLSAVE"]}]'] + (['mov r1, r1, lsr #8'] if s == 4 else ['and r1, r1, #255'])
                else:
                    src = get8(s, 'r1')
                l = ea_xy() + src + wr() + xynext(c)
            elif s == 6:   # LD r,(XY+d)
                if d in (4, 5):
                    l = ea_xy() + rd('r1', 'r0') + [f'ldr r0, [r4, #{O["HLSAVE"]}]'] + \
                        (['and r0, r0, #255', 'orr r0, r0, r1, lsl #8'] if d == 4 else ['bic r0, r0, #255', 'orr r0, r0, r1']) + \
                        [f'str r0, [r4, #{O["HLSAVE"]}]'] + xynext(c)
                else:
                    l = ea_xy() + rd('r1', 'r0') + set8(d, 'r1') + xynext(c)
            elif d in (4, 5) or s in (4, 5):
                l = get8(s, 'r1') + set8(d, 'r1') + xynext(c)
        elif x == 2 and (z in (4, 5, 6)):
            src = (ea_xy() + rd('r0', 'r0', 'r2')) if z == 6 else get8(z, 'r0')
            l = src + alu_v(ALU[y]) + xynext(c)
        elif op == 0xE1: l = pop('r9') + xynext(c)
        elif op == 0xE5: l = [f'str r9, [r4, #{O["TMP"]}]'] + push_tmp() + xynext(c)
        elif op == 0xE3: l = ex_sp('r9') + xynext(c)
        elif op == 0xE9: l = ['mov r5, r9'] + xynext(c)
        elif op == 0xF9: l = ['mov r12, r9'] + xynext(c)
        elif op == 0xCB: l = ['b z80a_xycb']
        if l is not None:
            H(nm, l)
            xy_tab[op] = nm

# ---------------------------------------------------------------- ED
def ed_block_loop(body, cond_more, name):
    """body performs one iteration; cond_more: lines that set flags so 'bne' means repeat"""
    top, done = L(), L()
    return [f'subs r6, r6, #16', f'{top}:'] + body + cond_more + [f'beq {done}', 'subs r6, r6, #5', 'ble 1f', 'sub r6, r6, #16', f'b {top}',
            '1:', 'sub r5, r5, #2', 'b z80a_exit', f'{done}:'] + next_(0)

def blk_pre(nm, target, need_b):
    """cheap eligibility test at the head of OUTI/OTIR: port C is a VDP data port (p & 0xC1 == 0x80) and the VDP access is not CRAM
    (OTIR also needs B == 0 (256) or B >= 4).  Eligible runs continue in the cold stub `target`, everything else falls into the per-byte
    code behind the label {nm}_gen."""
    b = ['mov r0, r10, lsr #8', 'sub r0, r0, #1', 'and r0, r0, #255', 'cmp r0, #3', f'blo {nm}_gen'] if need_b else []
    return b + ['and r0, r10, #0xC1', 'cmp r0, #0x80', f'bne {nm}_gen', f'ldrb r0, [r4, #{O["VCODE"]}]', 'cmp r0, #3', f'beq {nm}_gen',
                f'ldr pc, ={target}', f'{nm}_gen:']

def gen_ed():
    for op in range(256):
        c = CYCED[op]
        nm = f'ed_{op:02X}'; CUR[0] = nm
        l = None
        x, y, z = op >> 6, (op >> 3) & 7, op & 7
        if x == 1:
            if z == 0:     # IN r,(C)
                l = ['and r0, r10, #255', 'bl z80a_in_call', 'and r8, r8, #1', 'ldrb r1, [lr, r0]', 'orr r8, r8, r1'] + \
                    ([] if y == 6 else ['mov r1, r0'] + set8(y, 'r1')) + next_(c)
            elif z == 1:   # OUT (C),r
                l = (['mov r1, #0'] if y == 6 else get8(y, 'r1')) + ['and r0, r10, #255', 'bl z80a_out_call'] + next_(c)
            elif z == 2:   # SBC/ADC HL,rr
                v = ['r10', 'r11', 'r9', 'r12'][y >> 1]
                if y & 1 == 0:   # SBC
                    l = [SPS, f'mov r1, {v}', 'and r2, r8, #1', 'sub r0, r9, r1', 'sub r0, r0, r2',
                         'eor r2, r9, r1', 'eor r2, r2, r0', 'and r8, r2, #0x1000', 'mov r8, r8, lsr #8', 'orr r8, r8, #2',
                         'tst r0, #0x10000', 'orrne r8, r8, #1',
                         'eor r2, r1, r9', 'eor r12, r9, r0', 'and r2, r2, r12', 'and r2, r2, #0x8000', 'orr r8, r8, r2, lsr #13',
                         'and r2, r0, #0xA800', 'orr r8, r8, r2, lsr #8'] + mask16('r9', 'r0') + ['cmp r9, #0', 'orreq r8, r8, #0x40', SPL] + next_(c)
                else:            # ADC
                    l = [SPS, f'mov r1, {v}', 'and r2, r8, #1', 'add r0, r9, r1', 'add r0, r0, r2',
                         'eor r2, r9, r1', 'eor r2, r2, r0', 'and r8, r2, #0x1000', 'mov r8, r8, lsr #8',
                         'tst r0, #0x10000', 'orrne r8, r8, #1',
                         'eor r2, r1, r9', 'eor r2, r2, #0x8000', 'eor r12, r1, r0', 'and r2, r2, r12', 'and r2, r2, #0x8000', 'orr r8, r8, r2, lsr #13',
                         'and r2, r0, #0xA800', 'orr r8, r8, r2, lsr #8'] + mask16('r9', 'r0') + ['cmp r9, #0', 'orreq r8, r8, #0x40', SPL] + next_(c)
            elif z == 3:   # LD (nn),rr / LD rr,(nn)
                rr = ['r10', 'r11', 'r9', 'r12'][y >> 1]
                if y & 1 == 0:
                    l = fetch16('r0') + [f'and r1, {rr}, #255', f'str r0, [r4, #{O["TMP"]}]'] + wr() + [f'ldr r0, [r4, #{O["TMP"]}]', 'add r0, r0, #1'] + \
                        mask16('r0') + [f'mov r1, {rr}, lsr #8'] + wr() + next_(c)
                else:
                    l = fetch16('r1') + ['mov r0, r1'] + rd('r1', 'r0') + [f'str r1, [r4, #{O["TMP"]}]', 'add r0, r0, #1'] + mask16('r0') + rd('r1', 'r0') + \
                        [f'ldr r0, [r4, #{O["TMP"]}]', f'orr {rr}, r0, r1, lsl #8'] + next_(c)
            elif z == 4:   # NEG
                l = ['mov r0, r7', 'mov r7, #0'] + alu_v('SUB') + next_(c)
            elif z == 5:   # RETN / RETI
                l = pop('r5') + [f'ldrb r0, [r4, #{O["IFF2"]}]', f'strb r0, [r4, #{O["IFF1"]}]', f'subs r6, r6, #{c}', 'b z80a_arm_irq']
            elif z == 6:
                l = [f'mov r0, #{[0, 0, 1, 2, 0, 0, 1, 2][y]}', f'strb r0, [r4, #{O["IM"]}]'] + next_(c)
            elif op == 0x47: l = [f'strb r7, [r4, #{O["I"]}]'] + next_(c)
            elif op == 0x4F: l = [f'strb r7, [r4, #{O["R"]}]', 'and r0, r7, #0x80', f'strb r0, [r4, #{O["RHI"]}]'] + next_(c)
            elif op == 0x57:
                l = [f'ldrb r7, [r4, #{O["I"]}]'] + ld_air(c)
            elif op == 0x5F:
                l = ['and r7, r6, #0x7F', f'ldrb r0, [r4, #{O["RHI"]}]', 'orr r7, r7, r0'] + ld_air(c)
            elif op == 0x67:   # RRD
                l = rd('r1', 'r9') + ['and r0, r1, #15', 'mov r1, r1, lsr #4', 'orr r1, r1, r7, lsl #4', 'and r1, r1, #255',
                                      'bic r7, r7, #15', 'orr r7, r7, r0', 'and r8, r8, #1', 'ldrb r0, [lr, r7]', 'orr r8, r8, r0', 'mov r0, r9'] + wr() + next_(c)
            elif op == 0x6F:   # RLD
                l = rd('r1', 'r9') + ['mov r0, r1, lsr #4', 'and r2, r7, #15', 'orr r1, r2, r1, lsl #4', 'and r1, r1, #255',
                                      'bic r7, r7, #15', 'orr r7, r7, r0', 'and r8, r8, #1', 'ldrb r0, [lr, r7]', 'orr r8, r8, r0', 'mov r0, r9'] + wr() + next_(c)
        elif x == 2 and z <= 3 and y >= 4:
            inc = '#1' if y in (4, 6) else '#-1'
            rep = y >= 6
            if z == 0:     # LDI/LDD
                one = rd('r1', 'r9') + ['add r0, r1, r7', f'str r0, [r4, #{O["TMP2"]}]', 'mov r0, r11'] + wr() + \
                      [f'add r9, r9, {inc}' if inc == '#1' else 'sub r9, r9, #1'] + mask16('r9') + \
                      [f'add r11, r11, {inc}' if inc == '#1' else 'sub r11, r11, #1'] + mask16('r11') + \
                      ['sub r10, r10, #1'] + mask16('r10') + \
                      [f'ldr r1, [r4, #{O["TMP2"]}]', 'and r8, r8, #0xC1', 'cmp r10, #0', 'orrne r8, r8, #4', 'and r0, r1, #8', 'orr r8, r8, r0', 'tst r1, #2', 'orrne r8, r8, #0x20']
                l = (one + next_(c)) if not rep else ed_block_loop(one, ['cmp r10, #0'], nm)
            elif z == 1:   # CPI/CPD
                one = rd('r1', 'r9') + [f'add r9, r9, {inc}' if inc == '#1' else 'sub r9, r9, #1'] + mask16('r9') + ['sub r10, r10, #1'] + mask16('r10') + \
                      ['sub r0, r7, r1', 'and r0, r0, #255', 'and r8, r8, #1', 'orr r8, r8, #2', 'add r2, lr, #256', 'ldrb r2, [r2, r0]', 'bic r2, r2, #0x28', 'orr r8, r8, r2',
                       'eor r2, r7, r1', 'eor r2, r2, r0', 'and r2, r2, #0x10', 'orr r8, r8, r2', 'cmp r10, #0', 'orrne r8, r8, #4',
                       'tst r8, #0x10', 'subne r0, r0, #1', 'and r1, r0, #8', 'orr r8, r8, r1', 'tst r0, #2', 'orrne r8, r8, #0x20']
                if not rep:
                    l = one + next_(c)
                else:  # repeat while BC != 0 and not Z
                    top, done = L(), L()
                    l = ['subs r6, r6, #16', f'{top}:'] + one + ['cmp r10, #0', f'beq {done}', 'tst r8, #0x40', f'bne {done}',
                         'subs r6, r6, #5', 'ble 1f', 'sub r6, r6, #16', f'b {top}', '1:', 'sub r5, r5, #2', 'b z80a_exit', f'{done}:'] + next_(0)
            elif z == 2:   # INI/IND
                one = ['and r0, r10, #255', 'bl z80a_in_call', 'mov r1, r0', 'mov r0, r9'] + wr() + \
                      [f'add r9, r9, {inc}' if inc == '#1' else 'sub r9, r9, #1'] + mask16('r9') + ['sub r10, r10, #0x100'] + mask16('r10') + \
                      ['bic r8, r8, #0x40', 'orr r8, r8, #2', 'tst r10, #0xFF00', 'orreq r8, r8, #0x40']
                l = (one + next_(c)) if not rep else ed_block_loop(one, ['tst r10, #0xFF00'], nm)
            else:          # OUTI/OUTD
                one = rd('r1', 'r9') + ['sub r10, r10, #0x100'] + mask16('r10') + ['and r0, r10, #255', 'bl z80a_out_call'] + \
                      [f'add r9, r9, {inc}' if inc == '#1' else 'sub r9, r9, #1'] + mask16('r9') + \
                      ['bic r8, r8, #0x40', 'orr r8, r8, #2', 'tst r10, #0xFF00', 'orreq r8, r8, #0x40']
                l = (one + next_(c)) if not rep else ed_block_loop(one, ['tst r10, #0xFF00'], nm)
                if op in (0xA3, 0xB3):   # VDP data-port block output: hand 4+ byte runs to the block writer (z80a_blk_c in psc8.c)
                    l = blk_pre(nm, 'z80a_blk_outi' if op == 0xA3 else 'z80a_blk_otir', op == 0xB3) + l
        if l is not None:
            H(nm, l)
            ed_tab[op] = nm

def ld_air(c):
    return ['and r8, r8, #1', 'add r0, lr, #256', 'ldrb r0, [r0, r7]', 'orr r8, r8, r0', f'ldrb r0, [r4, #{O["IFF2"]}]', 'cmp r0, #0', 'orrne r8, r8, #4'] + next_(c)

import re as _re
_GLOBAL_TARGETS = ('z80a_dispatch', 'z80a_exit', 'z80a_arm_irq', 'z80a_wr8', 'z80a_in_call', 'z80a_in_a', 'z80a_out_call', 'z80a_xycb', 'z80a_cb', 'z80a_ed', 'z80a_xy')
def coldify(lines):
    out_l = []
    for ln in lines:
        m = _re.match(r'^(b|bl)(eq|ne|gt|le|lt|ge|cs|cc|hi|ls|mi|pl)?\s+(\S+)$', ln)
        if m and m.group(3) in _GLOBAL_TARGETS:
            cond = m.group(2) or ''
            if m.group(1) == 'bl':
                assert not cond
                out_l += ['mov lr, pc', f'ldr pc, ={m.group(3)}']
            else:
                out_l.append(f'ldr{cond} pc, ={m.group(3)}')
        else:
            out_l.append(ln)
    return out_l

# ---------------------------------------------------------------- emit
def emit_all():
    a('.syntax unified', '.cpu arm7tdmi', '.arm')
    a('.section .hot.z80a,"ax",%progbits', '.align 2')
    a('.global z80a_run', '.global z80a_ctx_size')
    # entry: r0 = budget (>0). returns remaining budget (<= 0) in r0
    a('z80a_run:', 'push {r4-r11, lr}', 'ldr r4, =z80a_ctx',
      f'str r0, [r4, #{O["BUDGET"]}]', 'mov r6, r0',
      f'ldr r7, [r4, #{O["A"]}]', f'ldr r8, [r4, #{O["F"]}]', f'ldr r9, [r4, #{O["HL"]}]', f'ldr r10, [r4, #{O["BC"]}]',
      f'ldr r11, [r4, #{O["DE"]}]', f'ldr r5, [r4, #{O["PC"]}]', f'ldr r12, [r4, #{O["SP"]}]',
      f'ldr r3, [r4, #{O["MAINTAB"]}]', f'add lr, r4, #{TAB}',
      f'ldrb r0, [r4, #{O["HALTED"]}]', 'cmp r0, #0', 'bne .Lentry_halted',
      f'ldrb r0, [r4, #{O["INTP"]}]', f'ldrb r1, [r4, #{O["IFF1"]}]', 'ands r0, r0, r1', 'beq .Lgo',
      # pending interrupt: accept it after the first instruction (as the previous core)
      f'str r6, [r4, #{O["SAVEDBUDGET"]}]', 'mov r6, #1', 'mov r0, #1', f'strb r0, [r4, #{O["IRQCHK"]}]',
      '.Lgo:', *next_(0)[2:] if False else [])
    # NEXT without subtracting (budget > 0 here)
    for l in fetch('r0', 'r1') + ['ldr pc, [r3, r0, lsl #2]']:
        a(l)
    a('.Lentry_halted:',
      f'ldrb r0, [r4, #{O["INTP"]}]', f'ldrb r1, [r4, #{O["IFF1"]}]', 'ands r0, r0, r1', 'bne .Lhalt_take',
      'add r0, r6, #3', 'bic r0, r0, #3', 'sub r6, r6, r0', 'b .Lstore',
      '.Lhalt_take:', 'sub r6, r6, #4', 'b .Ltake_irq')
    # exit / interrupt arming
    a('z80a_exit:', f'ldrb r0, [r4, #{O["IRQCHK"]}]', 'cmp r0, #0', 'bne .Lirqchk')
    a('.Lstore:', f'str r7, [r4, #{O["A"]}]', f'str r8, [r4, #{O["F"]}]', f'str r9, [r4, #{O["HL"]}]', f'str r10, [r4, #{O["BC"]}]',
      f'str r11, [r4, #{O["DE"]}]', 'mov r5, r5, lsl #16', 'mov r5, r5, lsr #16', f'str r5, [r4, #{O["PC"]}]', f'str r12, [r4, #{O["SP"]}]',
      'mov r0, r6', 'pop {r4-r11, lr}', 'bx lr')
    a('.Lirqchk:', 'mov r0, #0', f'strb r0, [r4, #{O["IRQCHK"]}]', f'ldr r0, [r4, #{O["SAVEDBUDGET"]}]', 'sub r1, r6, #1', 'add r6, r0, r1',
      f'ldrb r0, [r4, #{O["INTP"]}]', f'ldrb r1, [r4, #{O["IFF1"]}]', 'ands r0, r0, r1', 'beq .Lresume')
    a('.Ltake_irq:', 'mov r0, #0', f'strb r0, [r4, #{O["INTP"]}]', f'strb r0, [r4, #{O["IFF1"]}]', f'strb r0, [r4, #{O["IFF2"]}]',
      f'ldrb r0, [r4, #{O["HALTED"]}]', 'cmp r0, #0', 'movne r0, #0', f'strbne r0, [r4, #{O["HALTED"]}]', 'addne r5, r5, #1')
    for l in mask16('r1', 'r5') + [f'str r1, [r4, #{O["TMP"]}]'] + push_tmp():
        a(l)
    a(f'ldrb r0, [r4, #{O["IM"]}]', 'cmp r0, #2', 'beq .Lim2', 'mov r5, #0x38', 'cmp r0, #1', 'subeq r6, r6, #13', 'subne r6, r6, #11', 'b .Lresume')
    a('.Lim2:', f'ldrb r0, [r4, #{O["I"]}]', f'ldrb r1, [r4, #{O["INTDATA"]}]', 'orr r0, r1, r0, lsl #8')
    for l in rd('r5', 'r0', 'r2') + ['add r0, r0, #1'] + mask16('r0') + rd('r1', 'r0', 'r2') + ['orr r5, r5, r1, lsl #8', 'sub r6, r6, #19']:
        a(l)
    a('.Lresume:', 'cmp r6, #0', 'ble .Lstore', 'z80a_dispatch:')
    for l in fetch('r0', 'r1') + ['ldr pc, [r3, r0, lsl #2]']:
        a(l)
    # EI / RETN / RETI: budget already charged; if an interrupt is pending, check again after one more instruction
    a('z80a_arm_irq:', f'ldrb r0, [r4, #{O["IRQCHK"]}]', 'cmp r0, #0', 'bne 1f', f'ldrb r0, [r4, #{O["INTP"]}]', 'cmp r0, #0', 'beq 1f', 'cmp r6, #0', 'ble 1f',
      f'str r6, [r4, #{O["SAVEDBUDGET"]}]', 'mov r6, #1', 'mov r0, #1', f'strb r0, [r4, #{O["IRQCHK"]}]', 'b .Lresume', '1:')
    for l in next_(0):
        a(l)
    # write helper: r0 = addr (< 0x10000), r1 = value, r2 = return address
    a('z80a_wr8:', *(['b .Lwr_call'] if TEST else []), 'cmp r0, #0xC000', 'blo .Lwr_low', 'mov lr, r0, lsr #10', 'ldr lr, [r4, lr, lsl #2]', 'strb r1, [lr, r0]',
      'add lr, r0, #4', 'tst lr, #0x10000', f'add lr, r4, #{TAB}', 'moveq pc, r2', 'b .Lwr_call',
      '.Lwr_low:', 'cmp r0, #0x8000', 'movlo pc, r2', f'ldrb lr, [r4, #{O["SRAMEN"]}]', 'cmp lr, #0', f'add lr, r4, #{TAB}', 'moveq pc, r2',
      '.Lwr_call:', f'str r2, [r4, #{O["LINK"]}]', f'str r12, [r4, #{O["SPSAVE"]}]', f'str r3, [r4, #{O["LINK2"]}]',
      'ldr r2, =z80a_wr_slow', 'mov lr, pc', 'bx r2',
      f'ldr r3, [r4, #{O["LINK2"]}]', f'ldr r12, [r4, #{O["SPSAVE"]}]', f'add lr, r4, #{TAB}', f'ldr r2, [r4, #{O["LINK"]}]', 'mov pc, r2')
    # I/O helpers (called with bl): r0 = port [, r1 = value]; in returns r0. Preserve r1? no.
    # IN fast path: V counter (ports 0x40-0x7F even) and the constant-per-frame ports 0xC0-0xFF come from the context
    # V counter: computed from the cycles elapsed in this slice (slices may span many lines):
    # line = VLINE0 + (VOFF + BUDGET - r6) / 228   (exact: x*301401214 >> 36 for x < 2^18)
    a('z80a_in_call:', 'and r1, r0, #0xC1', 'cmp r1, #0x40', 'bne .Lin_tab',
      f'ldr r1, [r4, #{O["BUDGET"]}]', 'sub r1, r1, r6', f'ldr r2, [r4, #{O["VOFF"]}]', 'add r1, r1, r2',
      'ldr r0, =301401214', 'umull r2, r0, r1, r0', 'mov r0, r0, lsr #4', f'ldr r1, [r4, #{O["VLINE0"]}]', 'add r0, r0, r1',
      'cmp r0, #0xDA', 'subhi r0, r0, #6', 'and r0, r0, #255', 'b .Lin_fast', '.Lin_tab:',
      f'add r1, r4, #0x{FASTIN:X}', 'ldrb r2, [r1, r0]', 'cmp r2, #0', 'beq .Lin_slow', f'add r1, r4, #0x{VALIN:X}', 'ldrb r0, [r1, r0]', f'ldr r1, [r4, #{O["INCNT"]}]', 'add r1, r1, #1', f'str r1, [r4, #{O["INCNT"]}]',
      '.Lin_fast:', 'mov r2, lr', f'add lr, r4, #{TAB}', 'mov pc, r2')
    # Scanline-wait skip.  IN A,(n) that reads the V counter and is followed by  CP m ; JR cc,<back>  (cc = NZ/Z/NC/C) starts or
    # continues a loop that can only exit when the V counter reaches a given line (the loop body, e.g. a key poll, is not
    # looked at: nothing it reads changes inside a slice but the V counter).  The time the loop would spend spinning is
    # passed in one step: the cycle counter moves to the start of the line the loop is waiting for (or to the end of the slice
    # when it needs the frame to wrap), then the read returns the V counter of that moment, which satisfies the loop.
    # Only compare values m <= 0xD4 are handled (above that the V counter repeats values).  Falls into z80a_in_call.
    a('z80a_in_a:', 'and r1, r0, #0xC1', 'cmp r1, #0x40', 'bne .Lin_tab', 'b z80a_vwait')
    VWAIT = ['z80a_vwait:', 'stmfd sp!, {r3, lr}',
      'mov r1, r5, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'ldrb r0, [r1, r5]', 'cmp r0, #0xFE', 'bne .Lva_go',
      'add r0, r5, #3', 'ldrb r0, [r1, r0]', 'tst r0, #0x80', 'beq .Lva_go',
      'add r0, r5, #2', 'ldrb lr, [r1, r0]', 'sub r0, lr, #0x20', 'tst r0, #0xE7', 'bne .Lva_go',
      'add r0, r5, #1', 'ldrb r3, [r1, r0]', 'cmp r3, #0xD4', 'bhi .Lva_late',
      f'ldr r1, [r4, #{O["BUDGET"]}]', 'sub r1, r1, r6', f'ldr r0, [r4, #{O["VOFF"]}]', 'add r1, r1, r0',      # r1 = cycles since line VLINE0 began
      'ldr r0, =301401214', 'umull r2, r0, r1, r0', 'mov r0, r0, lsr #4', f'ldr r2, [r4, #{O["VLINE0"]}]', 'add r0, r0, r2',   # r0 = line
      'cmp r0, #0xDA', 'subhi r2, r0, #6', 'movls r2, r0',                                                          # r2 = V counter
      'cmp lr, #0x30', 'beq .Lva_nc', 'bhi .Lva_c', 'cmp lr, #0x28', 'beq .Lva_z',
      'cmp r2, r3', 'beq .Lva_go', 'blo .Lva_to', 'b .Lva_all',                                                    # NZ: loops while V != m
      '.Lva_z:', 'cmp r2, r3', 'bne .Lva_go', 'add r3, r3, #1', 'b .Lva_to',                                      # Z: loops while V == m, leaves at m+1
      '.Lva_nc:', 'cmp r2, r3', 'blo .Lva_go', 'b .Lva_all',                                                       # NC: loops while V >= m, leaves at the wrap
      '.Lva_c:', 'cmp r2, r3', 'bhs .Lva_go',                                                                      # C: loops while V < m
      '.Lva_to:', f'ldr r2, [r4, #{O["VLINE0"]}]', 'sub r2, r3, r2', 'mov r0, r2, lsl #8', 'sub r0, r0, r2, lsl #5', 'add r0, r0, r2, lsl #2',   # r0 = 228 * (line - VLINE0)
      'subs r0, r0, r1', 'ble .Lva_go', 'subs r6, r6, r0', 'movle r6, #0', 'b .Lva_go',
      '.Lva_all:', 'mov r6, #0', 'b .Lva_go',
      # Gaiden's IN 7E; CP D8; JR C,-6. Advance only complete 30-T
      # iterations before both the threshold and slice boundary; keep the
      # final IN/CP/JR in the ordinary core, preserving A/F/PC exactly.
      '.Lva_late:', 'cmp r3, #0xDA', 'bhi .Lva_go', 'cmp lr, #0x38', 'bne .Lva_go',
      'add r0, r5, #3', 'ldrb r0, [r1, r0]', 'cmp r0, #0xFA', 'bne .Lva_go',
      f'ldr r1, [r4, #{O["BUDGET"]}]', 'sub r1, r1, r6', f'ldr r0, [r4, #{O["VOFF"]}]', 'add r1, r1, r0',
      f'ldr r2, [r4, #{O["VLINE0"]}]', 'sub r2, r3, r2',
      'mov r0, r2, lsl #8', 'sub r0, r0, r2, lsl #5', 'add r0, r0, r2, lsl #2',
      'subs r0, r0, r1', 'ble .Lva_go', 'sub r1, r6, #1', 'cmp r0, r1', 'movhi r0, r1',
      'ldr r1, =0x88888889', 'umull r2, r1, r0, r1', 'mov r1, r1, lsr #4',
      'rsb r1, r1, r1, lsl #4', 'sub r6, r6, r1, lsl #1',
      '.Lva_go:', 'mov r0, #0x7E', 'ldmfd sp!, {r3, lr}', 'b z80a_in_call', '.ltorg']
    # OUT fast path: VDP data port VRAM writes (CRAM writes and everything else go to C), with the same dirty
    # tracking as the C path: tile bit, name-table entry bit, sprite-table flag.
    a('z80a_out_call:', 'and r2, r0, #0xC1', 'cmp r2, #0x81', 'beq .Lout_ctl', 'cmp r2, #0x80', 'bne .Lout_slow', f'ldrb r2, [r4, #{O["VCODE"]}]', 'cmp r2, #3', 'beq .Lout_slow',
      f'str lr, [r4, #{O["LINK"]}]', 'mov r2, #0', f'strb r2, [r4, #{O["CMDFIRST"]}]', f'ldr r0, [r4, #{O["VADDR"]}]',
      f'ldr r2, [r4, #{O["VRAMPTR"]}]', 'strb r1, [r2, r0]', f'strb r1, [r4, #{O["READBUF"]}]',
      f'ldr r2, [r4, #{O["TILEBITS"]}]', 'add r2, r2, r0, lsr #8', 'ldrb r1, [r2]', 'mov lr, r0, lsr #5', 'and lr, lr, #7', 'mov r3, #1',
      'orr r1, r1, r3, lsl lr', 'strb r1, [r2]',
      f'ldr r2, [r4, #{O["NB"]}]', 'sub r2, r0, r2', 'mov r2, r2, lsl #18', 'mov r2, r2, lsr #18', 'cmp r2, #2048', 'bhs 1f',
      f'ldr r1, [r4, #{O["MAPBITS"]}]', 'add r1, r1, r2, lsr #4', 'mov r2, r2, lsr #1', 'and r2, r2, #7', 'ldrb lr, [r1]', 'orr lr, lr, r3, lsl r2', 'strb lr, [r1]',
      '1:', f'ldr r2, [r4, #{O["SB"]}]', 'sub r2, r0, r2', 'mov r2, r2, lsl #18', 'mov r2, r2, lsr #18', 'cmp r2, #64', 'blo 2f',
      'sub r2, r2, #128', 'cmp r2, #128', 'bhs 3f', '2:', f'strb r3, [r4, #{O["SPRDIRTY"]}]',
      '3:', 'add r0, r0, #1', 'mov r0, r0, lsl #18', 'mov r0, r0, lsr #18', f'str r0, [r4, #{O["VADDR"]}]',
      f'ldr r3, [r4, #{O["MAINTAB"]}]', f'ldr r2, [r4, #{O["LINK"]}]', f'add lr, r4, #{TAB}', 'mov pc, r2')
    # VDP control port: address/code latch in the core; register writes (code 2) go to C
    a('.Lout_ctl:', f'ldrb r2, [r4, #{O["CMDFIRST"]}]', 'cmp r2, #0', 'bne 1f',
      f'strb r1, [r4, #{O["CMDLO"]}]', 'mov r2, #1', f'strb r2, [r4, #{O["CMDFIRST"]}]', 'mov r2, lr', f'add lr, r4, #{TAB}', 'mov pc, r2',
      '1:', 'mov r2, r1, lsr #6', 'cmp r2, #2', 'beq .Lout_slow',
      'mov r0, #0', f'strb r0, [r4, #{O["CMDFIRST"]}]', f'strb r2, [r4, #{O["VCODE"]}]', 'and r1, r1, #63', f'ldrb r0, [r4, #{O["CMDLO"]}]',
      'orr r0, r0, r1, lsl #8', 'cmp r2, #0', 'bne 2f',
      f'str lr, [r4, #{O["LINK"]}]', f'ldr r1, [r4, #{O["VRAMPTR"]}]', 'ldrb r1, [r1, r0]', f'strb r1, [r4, #{O["READBUF"]}]',
      'add r0, r0, #1', 'mov r0, r0, lsl #18', 'mov r0, r0, lsr #18', f'ldr lr, [r4, #{O["LINK"]}]',
      '2:', f'str r0, [r4, #{O["VADDR"]}]', 'mov r2, lr', f'add lr, r4, #{TAB}', 'mov pc, r2')
    for nm, fn in (('.Lin_slow', 'z80a_in'), ('.Lout_slow', 'z80a_out')):
        a(f'{nm}:', f'str lr, [r4, #{O["LINK"]}]', f'str r12, [r4, #{O["SPSAVE"]}]', f'str r3, [r4, #{O["LINK2"]}]',
          f'str r6, [r4, #{O["R6NOW"]}]', f'str r10, [r4, #{O["BC"]}]', f'str r11, [r4, #{O["DE"]}]', f'str r9, [r4, #{O["HL"]}]',
          f'ldr r2, =z80a_{fn[5:]}_c', 'mov lr, pc', 'bx r2',
          f'ldr r3, [r4, #{O["LINK2"]}]', f'ldr r12, [r4, #{O["SPSAVE"]}]', f'ldr r2, [r4, #{O["LINK"]}]', f'add lr, r4, #{TAB}',
          f'ldrb r1, [r4, #{O["BREAK"]}]', 'cmp r1, #0', 'movne r6, #0', 'mov pc, r2')
    # Idle-loop skip (called from taken JR NZ/JR Z with r0 = disp<<24, r5 = target; may clobber r1, r2; returns via lr).
    # A loop body that only re-reads memory nothing but an interrupt can change (interrupts are taken between slices)
    # would spend the rest of the slice in the loop, so the slice is ended (r6 = 0).
    #   disp -7: LD A,(nn) ; CP n        disp -6: LD A,(nn) ; OR A|AND A
    #   disp -3: CP (HL)                 disp -4: LD A,(HL) ; OR A|AND A      disp -5: LD A,(HL) ; CP n
    pats = [(0xF9, [(0, (0x3A,)), (3, (0xFE,))]), (0xFA, [(0, (0x3A,)), (3, (0xB7, 0xA7))]), (0xFD, [(0, (0xBE,))]),
            (0xFC, [(0, (0x7E,)), (1, (0xB7, 0xA7))]), (0xFB, [(0, (0x7E,)), (1, (0xFE,))])]
    a('z80a_idle:')
    for disp, checks in pats:
        nxt = L()
        a(f'cmp r0, #0x{disp:02X}000000', f'bne {nxt}')
        for off, vals in checks:
            if off:
                a(f'add r2, r5, #{off}', *rd('r2', 'r2', 'r1'))
            else:
                a(*rd('r2', 'r5', 'r1'))
            a(f'cmp r2, #0x{vals[0]:02X}', *[f'cmpne r2, #0x{v:02X}' for v in vals[1:]], 'movne pc, lr')
        a('mov r6, #0', 'mov pc, lr', f'{nxt}:')
    a('mov pc, lr')
    a('.ltorg')
    # CB: generic
    a('z80a_cb:')
    for l in fetch('r0', 'r1') + [f'str r0, [r4, #{O["TMP2"]}]', 'and r1, r0, #7', 'cmp r1, #6', 'beq .Lcb_mem']:
        a(l)
    a('cmp r0, #0x04', 'beq .Lcb_rlch', 'cmp r0, #0x3F', 'beq .Lcb_srla')
    a('ldr r2, =cb_get', 'ldr pc, [r2, r1, lsl #2]')
    # Common register-only CB operations; no generic getter/op/put dispatch.
    a('.Lcb_srla:', f'ldr r0, [r4, #{O["GAIDEN"]}]', 'cmp r0, #0', 'beq .Lcb_srla_one',
      'ldr r1, =0x4C0', 'cmp r5, r1', 'ldrne r1, =0x4D1', 'cmpne r5, r1', 'bne .Lcb_srla_one',
      'mov r1, r5, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'cmp r1, r0', 'bne .Lcb_srla_one',
      'cmp r6, #24', 'blt .Lcb_srla_one', 'and r1, r7, #4', 'mov r7, r7, lsr #3',
      'ldrb r8, [lr, r7]', 'orr r8, r8, r1, lsr #2', 'add r5, r5, #4', *next_(24),
      '.Lcb_srla_one:', 'and r1, r7, #1', 'mov r7, r7, lsr #1', 'ldrb r8, [lr, r7]', 'orr r8, r8, r1', *next_(8))
    a('.Lcb_rlch:', 'mov r0, r9, lsr #8', 'mov r0, r0, lsl #1', 'orr r0, r0, r0, lsr #8',
      'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'tst r0, #1', 'orrne r8, r8, #1',
      'and r9, r9, #255', 'orr r9, r9, r0, lsl #8',
      f'ldr r0, [r4, #{O["GAIDEN"]}]', 'cmp r0, #0', 'beq .Lcb_rlch_one',
      'ldr r1, =0x43B', 'cmp r5, r1', 'ldrne r1, =0x46E', 'cmpne r5, r1', 'bne .Lcb_rlch_one',
      'mov r1, r5, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'cmp r1, r0', 'bne .Lcb_rlch_one',
      'ldr r1, =0xFFFC', 'cmp r11, r1', 'bhs .Lcb_rlch_one',
      'cmp r6, #70', 'blt .Lcb_rlch_one', '.Lgaiden_body:', 'sub r6, r6, #8',
      'tst r8, #1', 'subne r6, r6, #12', 'bne .Lgaiden_put', 'sub r6, r6, #36',
      f'ldr r0, [r4, #{O["IX"]}]', *rd('r7', 'r0', 'r1'), 'add r0, r0, #1', *mask16('r0'), f'str r0, [r4, #{O["IX"]}]',
      '.Lgaiden_put:', 'mov r0, r11', 'mov r1, r7', *wr(),
      'add r11, r11, #1', *mask16('r11'), 'sub r6, r6, #13',
      'sub r0, r10, #256', 'and r0, r0, #0xFF00', 'and r10, r10, #255', 'orr r10, r10, r0',
      'ldr r1, =0x43B', 'cmp r5, r1', 'ldreq r5, =0x438', 'ldrne r5, =0x466',
      'cmp r0, #0', 'subne r6, r6, #13', 'bne .Lgaiden_repeat',
      'ldr r1, =0x438', 'cmp r5, r1', 'addeq r5, r5, #14', 'addne r5, r5, #19', 'sub r6, r6, #8', '.Lgaiden_done:', *next_(0),
      '.Lgaiden_repeat:', 'ldr r1, =0xFFFC', 'cmp r11, r1', 'bhs .Lgaiden_done', f'ldr r0, [r4, #{O["GAIDEN"]}]', 'ldr r1, [r4, #4]', 'cmp r0, r1', 'bne .Lgaiden_done',
      'ldr r1, =0x438', 'cmp r5, r1', 'bne .Lgaiden_repeat_second',
      'cmp r6, #74', 'blt .Lgaiden_done', 'and r7, r9, #255', 'sub r6, r6, #4', 'add r5, r5, #3', 'b .Lgaiden_rotate',
      '.Lgaiden_repeat_second:', 'cmp r6, #103', 'blt .Lgaiden_done',
      f'ldr r0, [r4, #{O["IY"]}]', *rd('r7', 'r0', 'r1'), 'add r0, r0, #1', *mask16('r0'), f'str r0, [r4, #{O["IY"]}]',
      'and r0, r9, #255', 'eor r7, r7, r0', 'sub r6, r6, #33', 'add r5, r5, #8',
      '.Lgaiden_rotate:', 'mov r0, r9, lsr #8', 'mov r0, r0, lsl #1', 'orr r0, r0, r0, lsr #8',
      'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'tst r0, #1', 'orrne r8, r8, #1',
      'and r9, r9, #255', 'orr r9, r9, r0, lsl #8', 'b .Lgaiden_body',
      '.Lcb_rlch_one:', *next_(8))

    a('.Lcb_mem:')
    for l in rd('r0', 'r9', 'r2'):
        a(l)
    a('.Lcb_op:', f'ldr r1, [r4, #{O["TMP2"]}]', 'mov r1, r1, lsr #3', 'ldr r2, =cb_ops', 'ldr pc, [r2, r1, lsl #2]')
    a('.Lcb_put:', f'ldr r1, [r4, #{O["TMP2"]}]', 'and r2, r1, #7', 'ldr r1, =cb_put', 'ldr pc, [r1, r2, lsl #2]')
    getters = []
    for i in range(8):
        if i == 6: getters.append('.Lcb_mem'); continue
        n = f'.Lcbg{i}'; getters.append(n)
        a(f'{n}:', *get8(i, 'r0') if i != 7 else ['mov r0, r7'], 'b .Lcb_op')
    putters = []
    for i in range(8):
        n = f'.Lcbp{i}'; putters.append(n)
        if i == 6:
            a(f'{n}:', 'mov r1, r0', 'mov r0, r9', *wr(), *next_(15))
        else:
            a(f'{n}:', *set8(i, 'r0'), *next_(8))
    ops = []
    rot = {
        0: ['mov r0, r0, lsl #1', 'orr r0, r0, r0, lsr #8', 'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'tst r0, #1', 'orrne r8, r8, #1'],   # RLC
        1: ['and r1, r0, #1', 'orr r0, r0, r0, lsl #8', 'mov r0, r0, lsr #1', 'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],  # RRC
        2: ['and r1, r8, #1', 'orr r0, r1, r0, lsl #1', 'mov r1, r0, lsr #8', 'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],  # RL
        3: ['and r1, r8, #1', 'orr r0, r0, r1, lsl #8', 'and r1, r0, #1', 'mov r0, r0, lsr #1', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],    # RR
        4: ['mov r1, r0, lsr #7', 'mov r0, r0, lsl #1', 'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],                       # SLA
        5: ['and r1, r0, #1', 'and r2, r0, #0x80', 'orr r0, r2, r0, lsr #1', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],                       # SRA
        6: ['mov r1, r0, lsr #7', 'mov r0, r0, lsl #1', 'orr r0, r0, #1', 'and r0, r0, #255', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],     # SLL
        7: ['and r1, r0, #1', 'mov r0, r0, lsr #1', 'ldrb r8, [lr, r0]', 'orr r8, r8, r1'],                                              # SRL
    }
    for g in range(32):
        n = f'.Lcbo{g}'; ops.append(n)
        if g < 8:
            a(f'{n}:', *rot[g], 'b .Lcb_put')
        elif g < 16:
            b = g - 8
            # BIT b: F = (F&C)|H|(v&0x28 for registers)|(m ? m&S : Z|P); (HL): no X/Y
            a(f'{n}:', 'and r8, r8, #1', 'orr r8, r8, #0x10', f'ldr r1, [r4, #{O["TMP2"]}]', 'and r1, r1, #7', 'cmp r1, #6',
              'andne r2, r0, #0x28', 'orrne r8, r8, r2', f'ands r2, r0, #{1 << b}', 'orreq r8, r8, #0x44', 'andne r2, r2, #0x80', 'orrne r8, r8, r2',
              'cmp r1, #6', 'beq 1f', *next_(8), '1:', *next_(12))
        elif g < 24:
            a(f'{n}:', f'bic r0, r0, #{1 << (g - 16)}', 'b .Lcb_put')
        else:
            a(f'{n}:', f'orr r0, r0, #{1 << (g - 24)}', 'b .Lcb_put')
    # DD/FD prefix: r1 = offset of IX/IY in ctx
    a('z80a_xy:', f'str r1, [r4, #{O["XYOFF"]}]', f'str r9, [r4, #{O["HLSAVE"]}]', 'ldr r9, [r4, r1]')
    for l in fetch('r0', 'r1'):
        a(l)
    a('ldr r1, =xy_table', 'ldr pc, [r1, r0, lsl #2]')
    # not an index instruction (or another prefix): undo the swap and run it as a plain opcode (+4 T)
    a('xy_plain:', f'ldr r9, [r4, #{O["HLSAVE"]}]', 'sub r6, r6, #4', 'ldr pc, [r3, r0, lsl #2]')
    # DD/FD CB d op (rare): generated into the ROM section
    _saved = list(out); out.clear()
    # DD/FD CB d op: r9 = XY
    a('z80a_xycb:')
    for l in ea_xy() + [f'str r0, [r4, #{O["TMP"]}]'] + fetch('r1', 'r2') + [f'str r1, [r4, #{O["TMP2"]}]',
              f'ldr r1, [r4, #{O["XYOFF"]}]', 'str r9, [r4, r1]', f'ldr r9, [r4, #{O["HLSAVE"]}]'] + rd('r0', 'r0', 'r2') + \
             [f'ldr r1, [r4, #{O["TMP2"]}]', 'mov r1, r1, lsr #3', 'ldr r2, =xcb_ops', 'ldr pc, [r2, r1, lsl #2]']:
        a(l)
    # xcb result: write back to (XY+d) and (undocumented) copy to register
    a('.Lxcb_put:', 'mov r1, r0', f'ldr r0, [r4, #{O["TMP"]}]', f'str r1, [r4, #{O["TMP"]}]', *wr(),
      f'ldr r0, [r4, #{O["TMP"]}]', f'ldr r1, [r4, #{O["TMP2"]}]', 'and r1, r1, #7', 'ldr r2, =xcb_copy', 'ldr pc, [r2, r1, lsl #2]')
    xcopy = []
    for i in range(8):
        n = f'.Lxcc{i}'; xcopy.append(n)
        a(f'{n}:', *([] if i == 6 else set8(i, 'r0')), *next_(23))
    xops = []
    for g in range(32):
        n = f'.Lxco{g}'; xops.append(n)
        if g < 8:
            a(f'{n}:', *rot[g], 'b .Lxcb_put')
        elif g < 16:
            b = g - 8
            a(f'{n}:', 'and r8, r8, #1', 'orr r8, r8, #0x10', f'ldr r1, [r4, #{O["TMP"]}]', 'mov r1, r1, lsr #8', 'and r1, r1, #0x28', 'orr r8, r8, r1',
              f'ands r2, r0, #{1 << b}', 'orreq r8, r8, #0x44', 'andne r2, r2, #0x80', 'orrne r8, r8, r2', *next_(20))
        elif g < 24:
            a(f'{n}:', f'bic r0, r0, #{1 << (g - 16)}', 'b .Lxcb_put')
        else:
            a(f'{n}:', f'orr r0, r0, #{1 << (g - 24)}', 'b .Lxcb_put')
    _xcb = list(out); out.clear(); out.extend(_saved)
    a('.section .text.z80a_xcb,"ax",%progbits', '.align 2')
    for ln in coldify([x.strip() for x in _xcb]):
        a(ln)
    a('.ltorg')
    a('.section .hot.z80a,"ax",%progbits', '.align 2')
    # ED prefix
    a('z80a_ed:')
    for l in fetch('r0', 'r1'):
        a(l)
    a('ldr r1, =ed_table', 'ldr pc, [r1, r0, lsl #2]')
    a('ed_nop:', *next_(8))
    a('op_ill:', *next_(4))
    a('.ltorg')
    RARE = {'op_27', 'op_76', 'op_D9', 'op_E3', 'op_37', 'op_3F', 'op_F3', 'op_FB'}
    # Handlers that never ran (or ran fewer than ~3000 times in 11000 frames of random play) in Phantasy Star I, Phantasy Star Adventure
    # and Phantasy Star Gaiden (tools/sms_host.c 'ops 1'): they run from ROM, which frees IWRAM for the loops that do matter.
    COLDX = {'op_D4', 'op_DC', 'op_E4', 'op_EC', 'op_F4', 'op_FC', 'op_CE', 'op_DE', 'op_8E', 'op_9E', 'op_86', 'op_88', 'op_89', 'op_8B', 'op_8D', 'op_96', 'op_98', 'op_99', 'op_9A', 'op_9B', 'op_9C', 'op_9D', 'op_BA', 'op_BB', 'op_BC', 'op_BD', 'op_BF', 'op_80', 'op_84', 'op_92', 'op_94', 'op_97', 'op_C7', 'op_E7', 'op_FF', 'xy_09', 'op_E0', 'op_E8', 'op_39', 'op_E2', 'op_15', 'op_1D', 'op_25', 'op_27', 'op_2D', 'op_31', 'op_1F'}
    RARE |= COLDX
    HOTX = {'ed_A3', 'ed_B3', 'ed_B0', 'op_27', 'xy_7E', 'xy_19', 'xy_77', 'xy_96', 'xy_BE', 'xy_46', 'xy_34', 'xy_36', 'xy_E5', 'xy_21', 'xy_86', 'xy_7D', 'xy_B6', 'xy_35', 'xy_70', 'xy_5E', 'xy_56', 'xy_E1', 'xy_09', 'xy_23'}
    HOTX -= COLDX
    hot = [(n, l) for n, l in handlers.items() if (n.startswith('op_') and n not in RARE) or n in HOTX]
    cold = [(n, l) for n, l in handlers.items() if not ((n.startswith('op_') and n not in RARE) or n in HOTX)]
    for k, (nm, lines) in enumerate(hot):
        a(f'{nm}:', *lines)
        if k % 40 == 39:
            a('.ltorg')
    a(*VWAIT)
    a('.ltorg')
    # XY and ED handlers run from ROM (IWRAM is too small); branches into IWRAM use literal loads
    a('.section .text.z80a_cold,"ax",%progbits', '.align 2')
    # VDP block writer entry (see psc8.c z80a_blk_c): z80a_blk_outi checks for "JP NZ,<the OUTI>" behind the OUTI (r5 = address after it)
    BLK = ['z80a_blk_outi:',
      'mov r1, r5, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'ldrb r0, [r1, r5]', 'cmp r0, #0xED', 'bne .Lbo_jp',
      'add r2, r5, #1', 'mov r1, r2, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'ldrb r1, [r1, r2]', 'cmp r1, #0xA3', 'bne .Lbo_no',
      'mov r0, #2', 'b z80a_blk',                                                   # OUTI directly followed by another OUTI (unrolled upload)
      '.Lbo_jp:', 'cmp r0, #0xC2', 'bne .Lbo_no',                                   # OUTI ; JP NZ,<the OUTI>  with B == 0 (256) or B >= 4
      'mov r1, r10, lsr #8', 'sub r1, r1, #1', 'and r1, r1, #255', 'cmp r1, #3', 'blo .Lbo_no',
      'add r2, r5, #1', 'mov r1, r2, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'ldrb r0, [r1, r2]',
      'add r2, r5, #2', 'mov r1, r2, lsr #10', 'ldr r1, [r4, r1, lsl #2]', 'ldrb r1, [r1, r2]', 'orr r0, r0, r1, lsl #8',
      'sub r1, r5, #2', 'mov r1, r1, lsl #16', 'mov r1, r1, lsr #16', 'cmp r0, r1', 'bne .Lbo_no',
      'mov r0, #1', 'b z80a_blk',
      '.Lbo_no:', 'ldr pc, =ed_A3_gen',
      'z80a_blk_otir:', 'mov r0, #0',
      'z80a_blk:', f'str r12, [r4, #{O["SPSAVE"]}]', f'str r3, [r4, #{O["LINK2"]}]', f'str r6, [r4, #{O["R6NOW"]}]', f'str r10, [r4, #{O["BC"]}]',
      f'str r9, [r4, #{O["HL"]}]', f'str r8, [r4, #{O["F"]}]', f'str r5, [r4, #{O["PC"]}]',
      'ldr r1, =z80a_blk_c', 'mov lr, pc', 'bx r1',
      f'ldr r3, [r4, #{O["LINK2"]}]', f'ldr r12, [r4, #{O["SPSAVE"]}]', f'ldr r6, [r4, #{O["R6NOW"]}]', f'ldr r10, [r4, #{O["BC"]}]',
      f'ldr r9, [r4, #{O["HL"]}]', f'ldr r8, [r4, #{O["F"]}]', f'ldr r5, [r4, #{O["PC"]}]', f'add lr, r4, #{TAB}',
      'cmp r6, #0', 'bgt z80a_dispatch', 'b z80a_exit', '.ltorg']
    a(*coldify(BLK))
    for k, (nm, lines) in enumerate(cold):
        a(f'{nm}:', *coldify(lines))
        if k % 30 == 29:
            a('.ltorg')
    a('.ltorg')
    a('.ltorg')
    # tables (IWRAM data)
    a('.section .hotdata.z80a,"aw",%progbits', '.align 2', '.global z80a_main_table')
    a('z80a_main_table:', *[f'.word {n}' for n in main_tab])
    a('.section .rodata.z80a,"a",%progbits', '.align 2')
    a('xy_table:', *[f'.word {n}' for n in xy_tab])
    a('ed_table:', *[f'.word {n}' for n in ed_tab])
    a('.section .hotdata.z80a,"aw",%progbits', '.align 2')
    a('cb_get:', *[f'.word {n}' for n in getters])
    a('cb_put:', *[f'.word {n}' for n in putters])
    a('cb_ops:', *[f'.word {n}' for n in ops])
    a('xcb_ops:', *[f'.word {n}' for n in xops])
    a('xcb_copy:', *[f'.word {n}' for n in xcopy])
    a('.section .hotdata.z80actx,"aw",%progbits', '.align 4', '.global z80a_ctx', f'z80a_ctx: .space {CTX_SIZE}')

gen_main()
gen_xy()
gen_ed()
CUR[0] = ''
emit_all()
open(sys.argv[1] if len(sys.argv) > 1 else 'z80a.s', 'w').write('\n'.join(out) + '\n')
hdr = ['/* generated by gen_asm.py */', '#ifndef Z80A_H', '#define Z80A_H', '#include <stdint.h>', f'#define Z80A_FASTIN 0x{FASTIN:X}', f'#define Z80A_VALIN 0x{VALIN:X}',
       f'#define Z80A_CTX_SIZE 0x{CTX_SIZE:X}', f'#define Z80A_TAB 0x{TAB:X}']
hdr += [f'#define Z80A_{k} 0x{v:X}' for k, v in O.items()]
hdr += ['extern uint8_t z80a_ctx[];', 'extern const uint32_t z80a_main_table[];', 'int32_t z80a_run(int32_t budget);',
        '#define Z80A_U8(o) (*(volatile uint8_t*)(z80a_ctx+(o)))', '#define Z80A_U32(o) (*(volatile uint32_t*)(z80a_ctx+(o)))', '#endif']
open(os.path.join(os.path.dirname(os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else 'z80a.s')), 'z80a.h'), 'w').write('\n'.join(hdr) + '\n')
print('handlers', len(handlers), 'lines', len(out))
