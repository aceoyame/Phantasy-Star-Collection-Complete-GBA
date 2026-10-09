#!/usr/bin/env python3
"""Static recompiler for the Genesis sound driver (CP09).

All eight text adventures upload the same 4070-byte Sega Z80 sound driver (FNV-1a 0xfdfaa135).  Interpreting it on the
ARM7 costs far more than the rest of the sound system together, so this tool translates its machine code into one C
function (z80drv.inc) that runs the *same* instructions with the *same* T-state timing:

  * every instruction body is taken verbatim from the verified PSC8 core (psc8/core/z80f_ops.inc + the OP_* macros of
    z80f.h), with the opcode bytes / immediates / displacements turned into constants; only control flow is written here
  * cycle and R-register counts are accumulated per straight-line run and added once (exact values are available to every
    memory/I-O access through TS / TE, the instruction's start / end time)
  * F is an ordinary local: the compiler removes the flag computations whose result is never read
  * code reached only dynamically (RET / JP (HL) targets that were never seen) or instructions that are not translated
    leave the translated code (status 2) and are executed by the interpreter until translated code is reached again
  * the driver never modifies its own code; a write into a translated byte is detected and permanently falls back to
    the interpreter (status 3)

usage: z80aot.py [driver.bin] [out.inc]       (inputs seed_pcs.txt / dyn_targets.txt live next to this file)
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'psc8', 'core'))
sys.path.insert(0, HERE)
import z80dis  # noqa: E402  (comments only)

CODE_LIMIT = 0x1000          # the driver lives in the first 4070 bytes; writes below this address are checked
TRACK_R = True               # keep the refresh register exact (only LD A,R reads it)

# --------------------------------------------------------------------------------------------- core tables
h = open(os.path.join(CORE, 'z80f.h')).read()
inc = open(os.path.join(CORE, 'z80f_ops.inc')).read()


def parse_arr(name):
    m = re.search(name + r'\[256\][^=]*=\s*\{([^}]*)\}', h)
    return [int(x) for x in m.group(1).split(',')]


CYC00, CYCED, CYCXY = parse_arr('z80f_cyc_00'), parse_arr('z80f_cyc_ed'), parse_arr('z80f_cyc_ddfd')


def parse_cases(macro):
    m = re.search(r'#define ' + macro + r'\s*\\\n(.*?)(?=\n#define|\Z)', inc, re.S)
    out = {}
    for line in m.group(1).split('\n'):
        line = line.strip()
        mm = re.match(r'case 0x([0-9A-Fa-f]{2}):(.*) break; \\?$', line)
        if mm:
            out[int(mm.group(1), 16)] = mm.group(2).strip()
    return out


MAIN, XY, ED = parse_cases('Z80F_MAIN_CASES'), parse_cases('Z80F_XY_CASES'), parse_cases('Z80F_ED_CASES')
CC = ['!(F&ZF)', '(F&ZF)', '!(F&CF)', '(F&CF)', '!(F&PF)', '(F&PF)', '!(F&SF)', '(F&SF)']
R8 = ['B', 'C', 'D', 'E', 'H', 'L', None, 'A']


class Insn:
    pass


def ulen(o):
    x, y, z, q = o >> 6, (o >> 3) & 7, o & 7, (o >> 3) & 1
    if x == 0:
        if z == 0:
            return 1 if y < 2 else 2
        if z == 1:
            return 3 if q == 0 else 1
        if z == 2:
            return 3 if (y >> 1) >= 2 else 1
        if z == 6:
            return 2
        return 1
    if x in (1, 2):
        return 1
    if z == 2 or z == 4:
        return 3
    if z == 3:
        return {0: 3, 1: 2, 2: 2, 3: 2}.get(y, 1)       # JP nn, CB, OUT, IN (CB is handled by the caller)
    if z == 5:
        return 3 if (q == 1 and (y >> 1) == 0) else 1
    if z == 6:
        return 2
    return 1


def decode(mem, pc):
    """-> Insn(addr, n, bytes, kind, ...).  kind: seq | jr | jp | call | ret | rst | jphl | reti | halt | bad"""
    i = Insn()
    i.addr = pc
    i.pre = ''
    i.cond = None
    i.target = None
    i.imm = []
    i.m1 = 1
    i.body = None
    i.kind = 'seq'
    b = lambda k: mem[(pc + k) & 0xffff]
    o = b(0)
    if o == 0xCB:
        i.pre, i.n, i.op, i.m1 = 'CB', 2, b(1), 2
        i.cyc = 8 if (i.op & 7) != 6 else (12 if (i.op & 0xc0) == 0x40 else 15)
    elif o == 0xED:
        op = b(1)
        i.pre, i.op, i.m1 = 'ED', op, 2
        if op not in ED:
            i.kind, i.n = 'bad', 2
            return i
        i.body = ED[op]
        i.n = 4 if 'FETCH16' in i.body else 2
        i.imm = [b(2), b(3)] if i.n == 4 else []
        i.cyc = CYCED[op]
        if op in (0x4D, 0x45, 0x55, 0x5D, 0x65, 0x6D, 0x75, 0x7D):
            i.kind = 'reti'
        elif op == 0xB0 or op == 0xB8:
            i.cyc = 16
        elif op in (0xB1, 0xB2, 0xB3, 0xBA, 0xBB, 0xB9, 0xA2, 0xAA, 0xA3, 0xAB):
            i.kind = 'bad'
        elif op in (0x40, 0x41, 0x48, 0x49, 0x50, 0x51, 0x58, 0x59, 0x60, 0x61, 0x68, 0x69, 0x70, 0x71, 0x78, 0x79):
            i.kind = 'bad'                                   # port I/O: not used by the driver
    elif o in (0xDD, 0xFD):
        k = 0
        extra = 0
        while b(k) in (0xDD, 0xFD):
            k += 1
        # (a prefix run longer than one: the last one counts, each extra one costs 4 T and one R)
        pref = b(k - 1)
        extra = k - 1
        op = b(k)
        i.pre = 'IX' if pref == 0xDD else 'IY'
        i.op = op
        i.m1 = k + 1
        if op == 0xCB:
            d, x = b(k + 1), b(k + 2)
            i.pre += 'CB'
            i.imm = [d, x]
            i.n = k + 3
            i.cyc = 4 * extra + (20 if (x & 0xc0) == 0x40 else 23)
            i.m1 = k + 1
        elif op in XY:
            i.body = XY[op]
            nimm = (1 if 'EA_XY' in i.body else 0) + (2 if 'FETCH16' in i.body else (1 if re.search(r'FETCH\(\)', i.body) else 0))
            i.imm = [b(k + 1 + j) for j in range(nimm)]
            i.n = k + 1 + nimm
            t = CYCXY[op]
            i.cyc = 4 * extra + (4 + CYC00[op] if t == 4 else t)
            if op == 0xE9:
                i.kind = 'jphl'
        else:
            i.kind, i.n = 'bad', k + 1
    else:
        i.op = o
        n = ulen(o)
        i.n = n
        i.imm = [b(1 + j) for j in range(n - 1)]
        i.cyc = CYC00[o]
        x, y, z = o >> 6, (o >> 3) & 7, o & 7
        imm16 = (i.imm[0] | (i.imm[1] << 8)) if len(i.imm) == 2 else None
        if o == 0x76:
            i.kind = 'halt'
        elif o == 0x10:
            i.kind, i.cond = 'jr', 'djnz'
            i.target = (pc + 2 + ((i.imm[0] ^ 128) - 128)) & 0xffff
        elif o == 0x18:
            i.kind = 'jr'
            i.target = (pc + 2 + ((i.imm[0] ^ 128) - 128)) & 0xffff
        elif o in (0x20, 0x28, 0x30, 0x38):
            i.kind, i.cond = 'jr', CC[y - 4]
            i.target = (pc + 2 + ((i.imm[0] ^ 128) - 128)) & 0xffff
        elif o == 0xC3:
            i.kind, i.target = 'jp', imm16
        elif x == 3 and z == 2:
            i.kind, i.cond, i.target = 'jp', CC[y], imm16
        elif o == 0xCD:
            i.kind, i.target = 'call', imm16
        elif x == 3 and z == 4:
            i.kind, i.cond, i.target = 'call', CC[y], imm16
        elif o == 0xC9:
            i.kind = 'ret'
        elif x == 3 and z == 0:
            i.kind, i.cond = 'ret', CC[y]
        elif x == 3 and z == 7:
            i.kind, i.target = 'rst', y * 8
        elif o == 0xE9:
            i.kind = 'jphl'
        elif o == 0xFB:
            i.kind = 'ei'
        elif o not in MAIN:
            i.kind = 'bad'
        else:
            i.body = MAIN[o]
    return i


def succs(i, mem):
    """-> (fallthrough address or None, [static targets], return point or None)"""
    nxt = (i.addr + i.n) & 0xffff
    k = i.kind
    if k in ('seq', 'ei'):
        return nxt, [], None
    if k == 'jr':
        return (nxt if i.cond else None), [i.target], None
    if k == 'jp':
        return (nxt if i.cond else None), [i.target], None
    if k == 'call':
        return nxt, [i.target], nxt
    if k == 'rst':
        return nxt, [i.target], nxt
    if k in ('ret', 'jphl', 'reti'):
        return (nxt if i.cond else None), [], None
    return None, [], None                                 # halt / bad: leave the translated code


# --------------------------------------------------------------------------------------------- discovery
def discover(mem, seeds, entries):
    insns = {}
    leaders = set(entries)
    work = list(seeds) + list(entries)
    while work:
        pc = work.pop()
        if pc in insns or pc >= CODE_LIMIT:
            continue
        i = decode(mem, pc)
        insns[pc] = i
        ft, tg, rp = succs(i, mem)
        if ft is not None:
            work.append(ft)
        for t in tg:
            work.append(t)
            leaders.add(t)
        if rp is not None:
            leaders.add(rp)
            work.append(rp)
    return insns, leaders


# --------------------------------------------------------------------------------------------- emission
def subst_body(body, i):
    """replace FETCH16()/FETCH()/EA_XY by the constants of the instruction (in order of appearance)."""
    imm = list(i.imm)

    def rep(m):
        t = m.group(0)
        if t == 'FETCH16()':
            lo, hi = imm.pop(0), imm.pop(0)
            return '0x%04x' % (lo | (hi << 8))
        if t == 'FETCH()':
            return '0x%02x' % imm.pop(0)
        d = (imm.pop(0) ^ 128) - 128                          # EA_XY
        return '((XY%+d)&0xffff)' % d if d else '(XY&0xffff)'
    out = re.sub(r'FETCH16\(\)|FETCH\(\)|EA_XY', rep, body)
    assert not imm, (i.addr, body, i.imm)
    return out


def cb_body(op, tgt=None, addr_expr=None, ixcb=False):
    """CB xx / DD CB d xx.  tgt: register letter or None for (HL)/(IX+d)."""
    r, n, x = op & 7, (op >> 3) & 7, op >> 6
    names = ['RLC', 'RRC', 'RL', 'RR', 'SLA', 'SRA', 'SLL', 'SRL']
    if not ixcb:
        if r != 6:
            reg = R8[r]
            if x == 0:
                return 'OP_%s(%s);' % (names[n], reg)
            if x == 1:
                return 'OP_BIT(%s, %d);' % (reg, n)
            if x == 2:
                return '%s &= ~(1u << %d);' % (reg, n)
            return '%s |= 1u << %d;' % (reg, n)
        a = '((H<<8)|L)'
        if x == 0:
            return '{uint32_t a_=%s, v=RD8(a_); OP_%s(v); WR8(a_, v);}' % (a, names[n])
        if x == 1:
            return '{uint32_t v=RD8(%s); OP_BITM(v, %d);}' % (a, n)
        if x == 2:
            return '{uint32_t a_=%s, v=RD8(a_); v &= ~(1u << %d); WR8(a_, v);}' % (a, n)
        return '{uint32_t a_=%s, v=RD8(a_); v |= 1u << %d; WR8(a_, v);}' % (a, n)
    # DD/FD CB d op : (undocumented register copy kept)
    cp = '' if r == 6 else ' %s = v;' % R8[r]
    if x == 0:
        return '{uint32_t a_=%s, v=RD8(a_); OP_%s(v); WR8(a_, v);%s}' % (addr_expr, names[n], cp)
    if x == 1:
        return '{uint32_t a_=%s, v=RD8(a_); OP_BITX(v, %d, a_);}' % (addr_expr, n)
    if x == 2:
        return '{uint32_t a_=%s, v=RD8(a_); v &= ~(1u << %d); WR8(a_, v);%s}' % (addr_expr, n, cp)
    return '{uint32_t a_=%s, v=RD8(a_); v |= 1u << %d; WR8(a_, v);%s}' % (addr_expr, n, cp)


class Gen:
    def __init__(self, mem, insns, leaders):
        self.mem, self.insns, self.leaders = mem, insns, leaders
        self.out = []
        self.pt = 0
        self.pr = 0

    def w(self, s):
        self.out.append(s)

    def flush(self):
        s = ''
        if self.pt:
            s += 'cyc += %d; ' % self.pt
        if self.pr and TRACK_R:
            s += 'R += %d; ' % self.pr
        if s:
            self.w('  ' + s.strip())
        self.pt = self.pr = 0

    def goto(self, target, frame_check=False, pre=''):
        """transfer to a static address: translated label or exit to the interpreter."""
        if target in self.insns and target in self.leaders:
            return '{ %sgoto L_%04x; }' % (pre, target)
        return '{ %sPC = 0x%04x; st = 2; goto exit; }' % (pre, target)

    def exit_to(self, pc, st, pend_t, pend_r):
        s = ''
        if pend_t:
            s += 'cyc += %d; ' % pend_t
        if pend_r and TRACK_R:
            s += 'R += %d; ' % pend_r
        return '{ %sPC = 0x%04x; st = %d; goto exit; }' % (s, pc, st)

    def macros(self, pt0, pt1):
        self.w('#undef TS\n#undef TE\n#define TS (cyc + %d)\n#define TE (cyc + %d)' % (pt0, pt1))

    def emit(self, i):
        pc, nxt = i.addr, (i.addr + i.n) & 0xffff
        mn = z80dis.dis(self.mem_w, pc)[1]
        raw = ' '.join('%02X' % self.mem[(pc + k) & 0xffff] for k in range(i.n))
        self.w('  /* %04x  %-11s %s */' % (pc, raw, mn))
        self.w('  Z80T(0x%04x, %d, %d);' % (pc, self.pt, self.pr))
        k = i.kind
        pt0, pr0 = self.pt, self.pr
        if k == 'bad' or k == 'halt':
            self.w('  ' + self.exit_to(pc, 2, pt0, pr0))
            self.dead = True
            return
        if k == 'seq' and i.pre == 'ED' and i.op in (0xB0, 0xB8):   # block move: unbounded time, left to the interpreter (boot only)
            self.w('  ' + self.exit_to(pc, 2, pt0, pr0))
            self.dead = True
            return
        if k == 'seq':
            self.emit_seq(i)
            return
        # control flow: flush including this instruction
        self.pt += i.cyc
        self.pr += i.m1
        if k == 'ei':
            self.w('  z->iff1 = z->iff2 = 1;')
            self.flush()
            self.w('  if (z->int_pending) { PC = 0x%04x; st = 1; goto exit; }' % nxt)
            if pc == IDLE_HOOK_AFTER:
                self.w('  IDLE_HOOK(0x%04x);' % nxt)
            return
        dflush = self.pt - pt0               # cycles added by the flush (start of this instruction = cyc - dflush afterwards)
        if k in ('jr', 'jp'):
            cond = i.cond
            if cond == 'djnz':
                self.flush()
                self.w('  B = (B - 1) & 255;')
                cond = 'B'
                tk = 5
            else:
                self.flush()
                tk = 5 if k == 'jr' else 0
            back = i.target <= pc
            pre = 'cyc += %d; ' % tk if tk else ''
            if cond is None:
                self.w('  ' + self.goto(i.target, back))
                self.dead = True
            else:
                self.w('  if (%s) %s' % (cond, self.goto(i.target, back, pre)))
            return
        if k == 'call' or k == 'rst':
            self.flush()
            self.w('#undef TS\n#undef TE\n#define TS (cyc - %d)\n#define TE cyc' % dflush)
            back = i.target <= pc
            tgt = self.goto(i.target, back)
            smc_exit = self.exit_to(i.target, 3, 0, 0)
            if i.target == 0x0434 and i.cond is None and os.environ.get('Z80_NATIVE_PITCH', '1') == '1':
                import native_leaf
                leaf,budget=native_leaf.pitch(self.mem,nxt,pc)
                assert self.mem[0x434:0x43e] == bytes.fromhex('0600dd7e10b7f23e0405')
                self.w('  { PUSH16(0x%04x); if (smc) %s' % (nxt, smc_exit))
                self.w('    if (IX>=0x1000u && IX<=0x1fefu && SP>=0x1000u && SP<=0x1ffeu && (int32_t)(cyc+%du-tend)<0) {' % budget)
                self.w(leaf)
                self.w('    } else %s }' % tgt)
                return
            if i.target in (0x0010,0x0018) and i.cond is None and os.environ.get('Z80_NATIVE_YM', '1') == '1':
                assert self.mem[0x10:0x18] == bytes.fromhex('ddcb017ec0c3a204')
                assert self.mem[0x4a2:0x4c9] == bytes.fromhex('ddcb0156200eddcb0056c0dd8601dfc9320140c9ddcb0056c0dd8601d604320240cf79320340c9')
                self.w('  { PUSH16(0x%04x); if (smc) %s' % (nxt, smc_exit))
                self.w('    if (SP >= 0x1004u && SP <= 0x3ffeu && (int32_t)(cyc + %du - tend) < 0) {' % (226 if i.target==0x10 else 99))
                self.w('      native_note.a=A; native_note.f=F; native_note.bc=(B<<8)|C; native_note.ix=IX; native_note.sp=SP; native_note.cyc=cyc; native_note.r=R;')
                self.w('      native_note.pc=0x%04x;' % i.target)
                self.w('#ifdef MD_HOST\n      native_note.hl=(H<<8)|L; native_note.de=(D<<8)|E; native_note.iy=IY;\n#endif')
                self.w('      native_ym_run(); A=native_note.a; F=native_note.f; cyc=native_note.cyc; R=native_note.r; SP=(SP+2)&0xffffu;')
                self.w('    } else %s }' % tgt)
                return
            if i.target == 0x0008 and i.cond is None:
                # The YM model exposes timer bits only: the busy bit is always
                # clear. Keep the original stack write, flags, refresh count,
                # timer read and instruction-boundary fallback, but return
                # directly to this native call site.
                assert self.mem[8:16] == bytes.fromhex('3a0040cb7f20f9c9')
                self.w('  { PUSH16(0x%04x); if (smc) %s' % (nxt, smc_exit))
                self.w('    if (SP <= 0x3ffeu && (int32_t)(cyc + 43 - tend) < 0) {')
                self.w('      Z80T(0x0008, 0, 0); timers_sync(cyc); A = ym_status;')
                self.w('      Z80T(0x000b, 13, 1); OP_BIT(A, 7);')
                self.w('      Z80T(0x000d, 21, 3); cyc += 28; R += 4;')
                self.w('      Z80T(0x000f, 0, 0); cyc += 10; R += 1; SP = (SP + 2) & 0xffffu;')
                self.w('    } else %s }' % tgt)
                return
            if i.cond is None:
                self.w('  { PUSH16(0x%04x); if (smc) %s %s }' % (nxt, smc_exit, tgt))
                self.dead = True
            else:
                self.w('  if (%s) { PUSH16(0x%04x); cyc += 7; if (smc) %s %s }' % (i.cond, nxt, smc_exit, tgt))
            return
        if k == 'ret':
            self.flush()
            self.w('#undef TS\n#undef TE\n#define TS (cyc - %d)\n#define TE cyc' % dflush)
            if i.cond is None:
                self.w('  PC = POP16(); goto dispatch;')
                self.dead = True
            else:
                self.w('  if (%s) { PC = POP16(); cyc += 6; goto dispatch; }' % i.cond)
            return
        if k == 'jphl':
            self.flush()
            xy = {'': '((H<<8)|L)', 'IX': 'IX', 'IY': 'IY'}[i.pre]
            self.w('  PC = %s; goto dispatch;' % xy)
            self.dead = True
            return
        if k == 'reti':
            self.flush()
            self.w('#undef TS\n#undef TE\n#define TS (cyc - %d)\n#define TE cyc' % dflush)
            self.w('  PC = POP16(); z->iff1 = z->iff2; if (z->int_pending && z->iff1) { st = 4; goto exit; } '
                   'goto dispatch;')
            self.dead = True
            return
        raise SystemExit('unhandled kind %s at %04x' % (k, pc))

    def emit_seq(self, i):
        pc, nxt = i.addr, (i.addr + i.n) & 0xffff
        pt0, pr0 = self.pt, self.pr
        self.pt += i.cyc
        self.pr += i.m1
        ptE = self.pt
        if i.pre == 'CB':
            body = cb_body(i.op)
        elif i.pre in ('IXCB', 'IYCB'):
            d = (i.imm[0] ^ 128) - 128
            xy = 'IX' if i.pre == 'IXCB' else 'IY'
            body = cb_body(i.imm[1], addr_expr=('((%s%+d)&0xffff)' % (xy, d)) if d else '(%s&0xffff)' % xy, ixcb=True)
        elif i.pre == 'ED' and i.op in (0xB0, 0xB8):
            dirn = 1 if i.op == 0xB0 else -1
            # the first iteration's 16 T is in pt; every repeat costs 21 T.  A write into translated code restarts the
            # instruction in the interpreter (the register state after each iteration is complete).
            body = 'for (;;) { OP_LDI(%d); if (smc) %s if (!(B | C)) break; cyc += 21; %s }' % (dirn, self.exit_to(pc, 3, pt0, pr0), 'R += 2;' if TRACK_R else '')
        elif i.pre in ('IX', 'IY'):
            body = subst_body(i.body, i)
            wb = ('%s = XY;' % i.pre) if re.search(r'XY=|OP_ADD16\(XY', i.body) else ''
            body = '{ uint32_t XY = %s; %s %s }' % (i.pre, body, wb)
        else:
            body = subst_body(i.body, i)
        uses_mem = bool(re.search(r'RD8|WR8|PUSH16|POP16|OP_LDI|OP_CPI', body))
        writes = bool(re.search(r'WR8|PUSH16|OP_LDI', body))
        if uses_mem:
            self.macros(pt0, ptE)
        if i.pre == 'ED' and i.op == 0x5F:                    # LD A,R : the value includes this instruction's refresh
            if TRACK_R:
                self.w('  R += %d;' % self.pr)
            self.pr = 0
        self.w('  ' + body)
        if writes:
            self.w('  if (smc) ' + self.exit_to(nxt, 3, self.pt, self.pr))


IDLE_HOOK_AFTER = 0x006F      # the main loop's EI: the idle skip is tried right after it


def bitmap(addrs, size=CODE_LIMIT):
    bm = bytearray(size // 8)
    for a in addrs:
        bm[a >> 3] |= 1 << (a & 7)
    return bm


def main():
    drv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'driver.bin')
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'z80drv.inc')
    drv = open(drv_path, 'rb').read()
    assert len(drv) == 4070
    mem = bytearray(65536)
    mem[:len(drv)] = drv
    seeds = [int(x, 16) for x in open(os.path.join(HERE, 'seed_pcs.txt')).read().split()]
    dyn = [int(l.split()[1], 16) for l in open(os.path.join(HERE, 'dyn_targets.txt')) if l.strip()]
    entries = [0x0000, 0x0038, 0x0066] + [t for t in dyn if t < CODE_LIMIT]
    insns, leaders = discover(mem, seeds, entries)
    leaders = {a for a in leaders if a in insns}
    ft_of = set()
    for i in insns.values():
        ft, _, _ = succs(i, mem)
        if ft is not None:
            ft_of.add(ft)
    leaders |= {a for a in insns if a not in ft_of}
    leaders |= {(i.addr + i.n) & 0xffff for i in insns.values() if i.kind == 'seq' and i.pre == 'ED' and i.op in (0xB0, 0xB8)
                and ((i.addr + i.n) & 0xffff) in insns}       # after a block move (interpreted) the translated code resumes
    # only the instructions that carry most of the execution (hot_pcs.txt, measured over all songs and effects) are translated;
    # the rest runs in the interpreter, which hands back at the next translated entry point
    hot_n = int(os.environ.get('Z80AOT_HOT', '736'))
    if hot_n:
        hot = set()
        for l in open(os.path.join(HERE, 'hot_pcs.txt')).read().split('\n')[:hot_n]:
            if l.strip():
                hot.add(int(l.split()[0], 16))
        pred = {}
        for i in insns.values():
            ft, _, _ = succs(i, mem)
            if ft is not None:
                pred[ft] = i.addr
        leaders = {a for a in leaders if a in hot} | {a for a in hot if a in insns and (a not in pred or pred[a] not in hot)}
        insns = {a: i for a, i in insns.items() if a in hot}
    g = Gen(mem, insns, leaders)
    g.mem_w = bytes(mem)
    g.dead = True
    code_bytes = set()
    for i in insns.values():
        for k in range(i.n):
            code_bytes.add(i.addr + k)
    order = sorted(insns)
    g.w('/* generated by z80aot.py from driver.bin - do not edit */')
    g.w('static const u8 z80drv_code[%d] = {%s};   /* bit per Z80 address < 0x%x: byte of a translated instruction */' %
        (CODE_LIMIT // 8, ','.join(str(b) for b in bitmap(code_bytes)), CODE_LIMIT))
    g.w('static const u8 z80drv_lead[%d] = {%s};   /* bit per address: translated entry point */' %
        (CODE_LIMIT // 8, ','.join(str(b) for b in bitmap(leaders))))
    g.w('#define Z80DRV_INSNS %d' % len(insns))
    g.w('#undef SYNC_OUT\n#define SYNC_OUT() ((void)0)')
    g.w('')
    g.w('/* status: 0 = frame time reached, 1 = EI with an interrupt pending (run one instruction, then accept), 2 = continue in')
    g.w(' * the interpreter, 3 = the driver wrote into its own code, 4 = RETI with an interrupt pending (accept now) */')
    g.w('ZAOT static u32 z80drv_run(Z80F *z, u32 tend) {')
    g.w('  uint32_t A = z->a, F = z->f, B = z->b, C = z->c, D = z->d, E = z->e, H = z->h, L = z->l;')
    g.w('  uint32_t SP = z->sp, IX = z->ix, IY = z->iy, R = z->r, cyc = z->cyc, PC = z->pc;')
    g.w('  uint32_t _fb, _fw, smc = 0, st = 0; (void)_fb; (void)_fw; (void)IX; (void)IY;')
    g.w('  goto dispatch;')
    seg = {}                      # label -> worst-case T-states of the straight-line run it starts (to the next label / dead end)
    cur = None
    for a in order:
        i = insns[a]
        if a in leaders:
            cur = a; seg[a] = 0
        extra = {'jr': 5, 'call': 7, 'ret': 6}.get(i.kind, 0) if getattr(i, 'cond', None) else 0
        seg[cur] += i.cyc + extra
    prev_end = None
    for a in order:
        i = insns[a]
        if a in leaders:
            if not g.dead:
                g.flush()
            g.pt = g.pr = 0
            g.w('L_%04x:' % a)
            # the frame interrupt is due inside this run: finish the frame in the interpreter, instruction by instruction
            g.w('  if ((int32_t)(cyc + %d - tend) >= 0) { PC = 0x%04x; st = 2; goto exit; }' % (seg[a], a))
            if a == 0x0f94 and os.environ.get('Z80_NATIVE_NOTE', '1') == '1':
                assert mem[0xf94:0xfb3] == bytes.fromhex('dd7717e5dd4e17cd8104e1cb7f2821fe83280cfe812813fe80280c030a18e1')
                assert mem[0x481:0x488] == bytes.fromhex('0600094d440ac9')
                g.w('  if (((IX + 0x17u) & 0xffffu) - 0x1000u < 0x3000u && SP >= 0x1004u && SP <= 0x4000u && (int32_t)(cyc + 201u - tend) < 0) {')
                g.w('    native_note.a=A; native_note.f=F; native_note.bc=(B<<8)|C; native_note.hl=(H<<8)|L;')
                g.w('    native_note.ix=IX; native_note.sp=SP; native_note.cyc=cyc; native_note.r=R;')
                g.w('#ifdef MD_HOST\n    native_note.de=(D<<8)|E; native_note.iy=IY;\n#endif')
                g.w('    native_note_run(tend); A=native_note.a; F=native_note.f; B=native_note.bc>>8; C=native_note.bc&255; cyc=native_note.cyc; R=native_note.r;')
                for target in [0xfc4, 0xfb3, 0xfbe, 0xfbb]:
                    g.w('    if (native_note.pc == 0x%04x) %s' % (target, g.goto(target)))
                g.w('    goto L_0f94;')
                g.w('  }')
            g.dead = False
        elif g.dead or prev_end != a:
            # reachable only as the fall-through of an instruction we could not follow: start a label-less run is impossible
            # (discover() adds every fall-through, so a non-leader after a dead end cannot occur)
            raise SystemExit('orphan instruction at %04x' % a)
        g.emit(i)
        prev_end = (a + i.n) & 0xffff
        # a run that falls through into an address we did not translate (never happens for decodable code)
        nxt = prev_end
        if not g.dead and (nxt not in insns):
            g.flush()
            g.w('  PC = 0x%04x; st = 2; goto exit;' % nxt)
            g.dead = True
    g.w('dispatch:')
    g.w('  switch (PC) {')
    for a in sorted(leaders):
        g.w('    case 0x%04x: goto L_%04x;' % (a, a))
    g.w('    default: st = 2; goto exit;')
    g.w('  }')
    g.w('exit:')
    g.w('  z->a = A; z->f = F; z->b = B; z->c = C; z->d = D; z->e = E; z->h = H; z->l = L;')
    g.w('  z->sp = SP; z->ix = IX; z->iy = IY; z->r = R; z->cyc = cyc; z->pc = PC;')
    g.w('  return st;')
    g.w('}')
    result = '\n'.join(g.out) + '\n'
    if os.environ.get('Z80AOT_WORDS', '0') == '1':
        # The native CPU has word registers: keep BC/DE/HL together instead
        # of rebuilding and splitting them around every pointer operation.
        old = '  uint32_t A = z->a, F = z->f, B = z->b, C = z->c, D = z->d, E = z->e, H = z->h, L = z->l;'
        new = '''  uint32_t A = z->a, F = z->f;
  union { uint32_t w; struct { uint8_t lo, hi, p0, p1; } b; }
    q_bc = { .w = ((uint32_t)z->b << 8) | z->c },
    q_de = { .w = ((uint32_t)z->d << 8) | z->e },
    q_hl = { .w = ((uint32_t)z->h << 8) | z->l };
#define B q_bc.b.hi
#define C q_bc.b.lo
#define D q_de.b.hi
#define E q_de.b.lo
#define H q_hl.b.hi
#define L q_hl.b.lo'''
        assert old in result
        result = result.replace(old, new)
        for hi, lo, q in [('B', 'C', 'q_bc'), ('D', 'E', 'q_de'), ('H', 'L', 'q_hl')]:
            for expr in [f'(({hi}<<8)|{lo})', f'({hi}<<8)|{lo}', f'({hi} << 8) | {lo}']:
                result = result.replace(expr, '(' + q + '.w)')
            for v in ['_t', 'h', 'v']:
                for src in [f'{hi}=({v}>>8)&255;{lo}={v}&255;', f'{hi}={v}>>8;{lo}={v}&255;']:
                    result = result.replace(src, f'{q}.w={v}&0xffffu;')
        result += '#undef B\n#undef C\n#undef D\n#undef E\n#undef H\n#undef L\n'
    open(out_path, 'w').write(result)
    print('translated %d instructions, %d entry points, %d code bytes -> %s (%d bytes of C)' %
          (len(insns), len(leaders), len(code_bytes), out_path, os.path.getsize(out_path)))


if __name__ == '__main__':
    main()
