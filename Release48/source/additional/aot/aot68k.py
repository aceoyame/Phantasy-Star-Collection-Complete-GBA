#!/usr/bin/env python3
"""Static recompiler: MC68000 code of the SEGANET text adventures -> C (compiled to Thumb for the GBA).

Semantics and cycle accounting are an exact transcription of additional/m68k_ref.c (the oracle the
interpreters are already checked against), specialised per instruction: every opcode field, extension word
and PC-relative address becomes a constant.  Anything the translator does not handle (privileged and SR
instructions, TRAP, line A/F, MOVEP, CHK, illegal encodings) is left to the interpreter: the generated code
returns AOT_INTERP with C.pc at that instruction and the dispatcher runs one reference step.

Execution model (see aotdisp.c):
  * code is grouped into regions (instructions connected by fall-through and direct branches; calls end the
    region edge).  A region is one C function taking an instruction index; every instruction is an entry.
  * before each instruction the cycle limit of the current m68k_exec call is checked, exactly like the
    interpreter's do{...}while(cycles<limit) loop, so interrupt and scanline timing are bit-identical.
  * leaving the region (call, return, indirect jump, branch to another region) sets C.pc and returns to the
    dispatcher.
  * backward branches of side-effect-free polling loops fold whole iterations up to the limit (same result as
    executing them; same rule as the interpreter's spin detector, decided statically plus a runtime C.fx check).

  python3 aot68k.py --rom GAME.bin --cover cov1.txt [cov2.txt ...] --range 0x0 0x2E000 --name eng --out eng.c
"""
import argparse, re, sys

M = {1: 0xFF, 2: 0xFFFF, 4: 0xFFFFFFFF}
SB = {1: 0x80, 2: 0x8000, 4: 0x80000000}
BITS = {1: 8, 2: 16, 4: 32}


def s8(v): return v - 0x100 if v & 0x80 else v
def s16(v): return v - 0x10000 if v & 0x8000 else v
def sz_from2(s): return {0: 1, 1: 2, 2: 4}.get(s, 0)
def hx(v): return '0x%Xu' % (v & 0xFFFFFFFF)


def const_ram(v):
    if not v.startswith('0x'): return None
    a = int(v[:-1], 16) & 0xFFFFFF
    return a & 0xFFFF if a >= 0xE00000 else None


def memrd(v, sz):
    o = const_ram(v)
    if o is None: return 'R%d(%s)' % (sz * 8, v)
    if sz == 1: return 'RAM8(0x%Xu)' % o
    o &= 0xFFFE
    if sz == 2: return 'RAM16(0x%Xu)' % o
    if o < 0xFFFC: return '(((u32)RAM16(0x%Xu)<<16)|RAM16(0x%Xu))' % (o, o + 2)
    return 'R32(%s)' % v


def memwr(v, sz, val):
    o = const_ram(v)
    if o is None: return 'W%d(%s,%s)' % (sz * 8, v, val)
    g = '' if o >= 0x8000 else ' aot_rgen[0x%X]++;' % (o >> 8)
    if sz == 1: return 'RAM8(0x%Xu)=(u8)(%s);%s' % (o, val, g)
    o &= 0xFFFE
    if sz == 2: return 'RAM16(0x%Xu)=(u16)(%s);%s' % (o, val, g)
    if o < 0xFFFC: return '{ u32 _w=(%s); RAM16(0x%Xu)=(u16)(_w>>16); RAM16(0x%Xu)=(u16)_w;%s%s }' % (val, o, o + 2, g, '' if o + 2 >= 0x8000 or ((o + 2) >> 8) == (o >> 8) else ' aot_rgen[0x%X]++;' % ((o + 2) >> 8))
    return 'W32(%s,%s)' % (v, val)


class Fallback(Exception):
    pass


class Insn:
    def __init__(self, addr):
        self.addr = addr
        self.c = []           # C statements; tokens: JUMP{addr} (direct transfer), EXITV{expr} (dynamic pc), FOLD{addr}
        self.succ = []        # static successors inside the code (fall-through + branch targets)
        self.fall = True      # control may fall through to addr+len
        self.call = None      # JSR/BSR absolute target (a separate entry)
        self.cyc = 0          # constant cycles (None if data dependent)
        self.memw = False     # writes memory (or pushes)
        self.anmod = False    # modifies An through (An)+/-(An)
        self.rd = set()       # registers read ('d0'..'a7')
        self.wr = []          # registers written, in order: (reg, partial, srcs)
        self.interp = False
        self.length = 2
        self.text = ''
        self.cmaxv = 200      # worst-case cycles when cyc is None

    def cmax(self):
        if self.interp: return 0
        if self.cyc is not None: return self.cyc
        return self.cmaxv


class Tx:
    def __init__(self, rom, base=0):
        self.rom = rom
        self.base = base      # 68k address of rom[0]

    def w16(self, a):
        o = a - self.base
        if o < 0 or o + 2 > len(self.rom): raise Fallback('outside code image')
        return (self.rom[o] << 8) | self.rom[o + 1]

    def w32(self, a): return (self.w16(a) << 16) | self.w16(a + 2)

    # ------------------------------------------------------------------ effective address
    def ea_get(self, I, st, mode, reg, sz):
        """returns ('D',r) ('A',r) ('M',cexpr) ('I',const); emits side effects and cycles in order"""
        n = st['tmp']; st['tmp'] += 1
        if mode == 0: I.rd.add('d%d' % reg); return ('D', reg)
        if mode == 1: I.rd.add('a%d' % reg); return ('A', reg)
        if mode == 2:
            I.rd.add('a%d' % reg); I.c.append('u32 ea%d=AA[%d];' % (n, reg)); return ('M', 'ea%d' % n)
        if mode == 3:
            inc = 2 if (reg == 7 and sz == 1) else sz
            I.rd.add('a%d' % reg); I.anmod = True; I.wr.append(('a%d' % reg, False, {'a%d' % reg}))
            I.c.append('u32 ea%d=AA[%d]; AA[%d]+=%du;' % (n, reg, reg, inc)); return ('M', 'ea%d' % n)
        if mode == 4:
            dec = 2 if (reg == 7 and sz == 1) else sz
            I.rd.add('a%d' % reg); I.anmod = True; I.wr.append(('a%d' % reg, False, {'a%d' % reg}))
            I.c.append('AA[%d]-=%du; u32 ea%d=AA[%d]; cyc+=2;' % (reg, dec, n, reg)); I.cyc += 2; return ('M', 'ea%d' % n)
        if mode == 5:
            d = s16(self.w16(st['pc'])); st['pc'] += 2; I.rd.add('a%d' % reg)
            I.c.append('u32 ea%d=AA[%d]+%s; cyc+=4;' % (n, reg, hx(d))); I.cyc += 4; return ('M', 'ea%d' % n)
        if mode == 6:
            x = self.index(I, st)
            I.rd.add('a%d' % reg)
            I.c.append('u32 ea%d=AA[%d]+%s; cyc+=6;' % (n, reg, x)); I.cyc += 6; return ('M', 'ea%d' % n)
        if mode == 7:
            if reg == 0:
                a = s16(self.w16(st['pc'])) & 0xFFFFFFFF; st['pc'] += 2
                I.c.append('cyc+=4;'); I.cyc += 4; return ('M', hx(a))
            if reg == 1:
                a = self.w32(st['pc']); st['pc'] += 4
                I.c.append('cyc+=8;'); I.cyc += 8; return ('M', hx(a))
            if reg == 2:
                b = st['pc']; a = (b + s16(self.w16(st['pc']))) & 0xFFFFFF; st['pc'] += 2
                I.c.append('u32 ea%d=hi|%s; cyc+=4;' % (n, hx(a))); I.cyc += 4; return ('M', 'ea%d' % n)
            if reg == 3:
                b = st['pc']; x = self.index(I, st)
                I.c.append('u32 ea%d=(hi|%s)+%s; cyc+=6;' % (n, hx(b), x)); I.cyc += 6; return ('M', 'ea%d' % n)
            if reg == 4:
                if sz == 4: v = self.w32(st['pc']); st['pc'] += 4
                else:
                    w = self.w16(st['pc']); st['pc'] += 2; v = (w & 0xFF) if sz == 1 else w
                I.c.append('cyc+=4;'); I.cyc += 4; return ('I', v)
        return ('I', 0)                                  # mode 7 reg 5..7: reference treats as immediate 0

    def index(self, I, st):
        e = self.w16(st['pc']); st['pc'] += 2
        r = (e >> 12) & 7; disp = s8(e & 0xFF)
        if e & 0x8000:
            I.rd.add('a%d' % r); x = 'AA[%d]' % r if e & 0x800 else '(u32)(s32)(s16)AA[%d]' % r
        else:
            I.rd.add('d%d' % r); x = 'AD[%d]' % r if e & 0x800 else '(u32)(s32)(s16)AD[%d]' % r
        return '(%s+%s)' % (x, hx(disp))

    def ea_rd(self, I, e, sz):
        k, v = e
        if k == 'D': return '(AD[%d]&%s)' % (v, hx(M[sz]))
        if k == 'A': return '(AA[%d]&%s)' % (v, hx(M[sz]))
        if k == 'M':
            c = 8 if sz == 4 else 4; I.cyc += c
            t = 'r%d' % len(I.c)
            I.c.append('cyc+=%d; u32 %s=%s;' % (c, t, memrd(v, sz))); return t
        return hx(v & M[sz])

    def ea_wr(self, I, e, sz, val, srcs=()):
        k, v = e
        if k == 'D':
            I.wr.append(('d%d' % v, sz != 4, set(srcs)))
            if sz == 4: I.c.append('AD[%d]=%s;' % (v, val))
            else: I.c.append('AD[%d]=(AD[%d]&~%s)|((%s)&%s);' % (v, v, hx(M[sz]), val, hx(M[sz])))
        elif k == 'A':
            I.wr.append(('a%d' % v, False, set(srcs))); I.c.append('AA[%d]=%s;' % (v, val))
        elif k == 'M':
            c = 8 if sz == 4 else 4; I.cyc += c; I.memw = True
            I.c.append('cyc+=%d; %s;' % (c, memwr(v, sz, val)))

    def regsrc(self, e):
        return {('d%d' if e[0] == 'D' else 'a%d') % e[1]} if e[0] in 'DA' else set()

    def push32(self, I, val):
        I.memw = True; I.rd.add('a7'); I.wr.append(('a7', False, {'a7'}))
        I.c.append('AA[7]-=4; W32(AA[7],%s);' % val)

    def cond(self, c):
        return '1' if c == 0 else '0' if c == 1 else 'CC(sr,%d)' % c

    def nz(self, I, v, sz):
        I.c.append('NZ(%s,%s,%s);' % (v, hx(M[sz]), hx(SB[sz])))

    # ------------------------------------------------------------------ decode + emit one instruction
    def insn(self, addr):
        I = Insn(addr)
        try:
            op = self.w16(addr)
            st = {'pc': addr + 2, 'tmp': 0}
            I.c.append('cyc+=4;'); I.cyc = 4
            self.dispatch(I, st, op)
            I.length = st['pc'] - addr
        except Fallback:
            I = Insn(addr); I.interp = True; I.fall = False; I.c = []; I.cyc = None; I.length = 2
        if I.fall: I.succ.append(addr + I.length)
        return I

    def dispatch(self, I, st, op):
        hi = op >> 12
        if hi == 0: self.line0(I, st, op)
        elif hi in (1, 2, 3):
            sz = 1 if hi == 1 else 2 if hi == 3 else 4
            s = self.ea_get(I, st, (op >> 3) & 7, op & 7, sz); v = self.ea_rd(I, s, sz)
            dm, dr = (op >> 6) & 7, (op >> 9) & 7
            if dm == 1:
                I.rd.discard('a%d' % dr)
                val = '(u32)(s32)(s16)%s' % v if sz == 2 else v
                I.wr.append(('a%d' % dr, False, self.regsrc(s))); I.c.append('AA[%d]=%s;' % (dr, val)); return
            I.c.append('u32 mv=%s;' % v)
            d = self.ea_get(I, st, dm, dr, sz)
            if d[0] == 'D': I.rd.discard('d%d' % dr) if sz == 4 else None
            self.ea_wr(I, d, sz, 'mv', self.regsrc(s)); self.nz(I, 'mv', sz)
        elif hi == 4: self.line4(I, st, op)
        elif hi == 5: self.line5(I, st, op)
        elif hi == 6: self.line6(I, st, op)
        elif hi == 7:
            if op & 0x100: raise Fallback()
            v = s8(op & 0xFF) & 0xFFFFFFFF; r = (op >> 9) & 7
            I.wr.append(('d%d' % r, False, set())); I.c.append('AD[%d]=%s;' % (r, hx(v))); self.nz(I, hx(v), 4)
        elif hi == 8: self.line8(I, st, op)
        elif hi == 9: self.addsub(I, st, op, 1)
        elif hi == 11: self.lineB(I, st, op)
        elif hi == 12: self.lineC(I, st, op)
        elif hi == 13: self.addsub(I, st, op, 0)
        elif hi == 14: self.lineE(I, st, op)
        else: raise Fallback()

    def bitop(self, I, st, mode, reg, t, bitexpr, bitsrc):
        if mode == 0:
            I.rd.add('d%d' % reg)
            I.c.append('{ u32 bn=(%s)&31u; u32 v=AD[%d]; if(v&(1u<<bn)) sr&=~FZ_; else sr|=FZ_;' % (bitexpr, reg))
            if t == 1: I.c.append('AD[%d]^=1u<<bn;' % reg)
            elif t == 2: I.c.append('AD[%d]&=~(1u<<bn);' % reg)
            elif t == 3: I.c.append('AD[%d]|=1u<<bn;' % reg)
            I.c.append('}')
            if t: I.wr.append(('d%d' % reg, True, {'d%d' % reg} | bitsrc))
            return
        e = self.ea_get(I, st, mode, reg, 1)
        I.c.append('u32 bn=(%s)&7u;' % bitexpr)
        v = self.ea_rd(I, e, 1)
        I.c.append('if(%s&(1u<<bn)) sr&=~FZ_; else sr|=FZ_;' % v)
        if t == 1: self.ea_wr(I, e, 1, '%s^(1u<<bn)' % v)
        elif t == 2: self.ea_wr(I, e, 1, '%s&~(1u<<bn)' % v)
        elif t == 3: self.ea_wr(I, e, 1, '%s|(1u<<bn)' % v)

    def line0(self, I, st, op):
        mode, reg = (op >> 3) & 7, op & 7
        if op & 0x100:
            if mode == 1: raise Fallback()                 # MOVEP
            dr = (op >> 9) & 7; I.rd.add('d%d' % dr)
            self.bitop(I, st, mode, reg, (op >> 6) & 3, 'AD[%d]' % dr, {'d%d' % dr}); return
        which = (op >> 9) & 7
        if which == 4:
            bitn = self.w16(st['pc']) & 0xFF; st['pc'] += 2
            self.bitop(I, st, mode, reg, (op >> 6) & 3, '%du' % bitn, set()); return
        sz = sz_from2((op >> 6) & 3)
        if not sz: raise Fallback()
        if mode == 7 and reg == 4:
            if sz != 1 or which not in (0, 1, 5): raise Fallback()
            imm = self.w16(st['pc']); st['pc'] += 2
            if which == 0: I.c.append('sr|=%s;' % hx(imm & 0x1F))
            elif which == 1: I.c.append('sr&=%s;' % hx(0xFF00 | imm))
            else: I.c.append('sr^=%s;' % hx(imm & 0x1F))
            return
        if which == 7: raise Fallback()
        if sz == 4: imm = self.w32(st['pc']); st['pc'] += 4
        else: imm = self.w16(st['pc']) & M[sz]; st['pc'] += 2
        e = self.ea_get(I, st, mode, reg, sz); d = self.ea_rd(I, e, sz)
        m, sb, src = hx(M[sz]), hx(SB[sz]), self.regsrc(e)
        if which == 0: I.c.append('u32 r=%s|%s;' % (d, hx(imm))); self.nz(I, 'r', sz); self.ea_wr(I, e, sz, 'r', src)
        elif which == 1: I.c.append('u32 r=%s&%s;' % (d, hx(imm))); self.nz(I, 'r', sz); self.ea_wr(I, e, sz, 'r', src)
        elif which == 2: I.c.append('u32 r=SUB(%s,%s,%s,%s,0,1,0);' % (hx(imm), d, m, sb)); self.ea_wr(I, e, sz, 'r', src)
        elif which == 3: I.c.append('u32 r=ADD(%s,%s,%s,%s,%d,0,1,0);' % (hx(imm), d, m, sb, BITS[sz])); self.ea_wr(I, e, sz, 'r', src)
        elif which == 5: I.c.append('u32 r=%s^%s;' % (d, hx(imm))); self.nz(I, 'r', sz); self.ea_wr(I, e, sz, 'r', src)
        elif which == 6: I.c.append('CMP(%s,%s,%s,%s);' % (hx(imm), d, m, sb))
        else: raise Fallback()

    def ctl(self, mode, reg): return mode in (2, 5, 6) or (mode == 7 and reg <= 3)

    def line4(self, I, st, op):
        mode, reg = (op >> 3) & 7, op & 7
        if (op & 0xF1C0) == 0x41C0:                       # LEA
            e = self.ea_get(I, st, mode, reg, 4); ar = (op >> 9) & 7
            if e[0] != 'M': raise Fallback()
            I.wr.append(('a%d' % ar, False, set(I.rd))); I.c.append('AA[%d]=%s;' % (ar, e[1])); return
        if (op & 0xF1C0) == 0x4180: raise Fallback()      # CHK
        k = op & 0xFFC0
        if k == 0x40C0:                                   # MOVE from SR
            e = self.ea_get(I, st, mode, reg, 2); self.ea_wr(I, e, 2, '(sr&0xFFFFu)'); return
        if k == 0x44C0:                                   # MOVE to CCR
            e = self.ea_get(I, st, mode, reg, 2); v = self.ea_rd(I, e, 2)
            I.c.append('sr=(sr&0xFF00u)|((%s)&0x1Fu);' % v); return
        if k == 0x46C0: raise Fallback()                  # MOVE to SR
        if k == 0x4800:                                   # NBCD
            e = self.ea_get(I, st, mode, reg, 1); v = self.ea_rd(I, e, 1)
            I.c.append('u32 r=f_bcd_sub(&sr,(%s)&0xFFu,0); sr=(sr&~FN_)|((r&0x80u)?FN_:0u);' % v); self.ea_wr(I, e, 1, 'r', self.regsrc(e)); return
        if k == 0x4840:
            if mode == 0:
                I.rd.add('d%d' % reg); I.wr.append(('d%d' % reg, False, {'d%d' % reg}))
                I.c.append('{ u32 v=AD[%d]; v=(v>>16)|(v<<16); AD[%d]=v; NZ(v,0xFFFFFFFFu,0x80000000u); }' % (reg, reg)); return
            e = self.ea_get(I, st, mode, reg, 4)
            if e[0] != 'M': raise Fallback()
            self.push32(I, e[1]); return                  # PEA
        if k == 0x4880 and mode == 0:
            I.rd.add('d%d' % reg); I.wr.append(('d%d' % reg, True, {'d%d' % reg}))
            I.c.append('{ u32 v=(u32)(s32)(s8)AD[%d]; AD[%d]=(AD[%d]&0xFFFF0000u)|(v&0xFFFFu); NZ(v,0xFFFFu,0x8000u); }' % (reg, reg, reg)); return
        if k == 0x48C0 and mode == 0:
            I.rd.add('d%d' % reg); I.wr.append(('d%d' % reg, False, {'d%d' % reg}))
            I.c.append('{ u32 v=(u32)(s32)(s16)AD[%d]; AD[%d]=v; NZ(v,0xFFFFFFFFu,0x80000000u); }' % (reg, reg)); return
        if k in (0x4880, 0x48C0, 0x4CC0, 0x4C80): self.movem(I, st, op); return
        if k == 0x4AC0:                                   # TAS
            if op == 0x4AFC: raise Fallback()
            e = self.ea_get(I, st, mode, reg, 1); v = self.ea_rd(I, e, 1); self.nz(I, v, 1); self.ea_wr(I, e, 1, '(%s|0x80u)' % v, self.regsrc(e)); return
        if k == 0x4E80:                                   # JSR
            e = self.ea_get(I, st, mode, reg, 4)
            if e[0] != 'M': raise Fallback()
            self.push32(I, '(hi|%s)' % hx(st['pc'])); I.c.append('cyc+=8;'); I.cyc += 8; I.fall = False
            if e[1].startswith('0x'):
                t = int(e[1][:-1], 16); I.call = t & 0xFFFFFF; I.c.append('JUMPA{%X}' % t)
            else: I.c.append('EXITV{%s}' % e[1])
            return
        if k == 0x4EC0:                                   # JMP
            e = self.ea_get(I, st, mode, reg, 4)
            if e[0] != 'M': raise Fallback()
            I.fall = False
            if e[1].startswith('0x'):
                t = int(e[1][:-1], 16); I.succ.append(t & 0xFFFFFF); I.c.append('JUMPA{%X}' % t)
            else: I.c.append('EXITV{%s}' % e[1])
            return
        h = op & 0xFF00
        if h in (0x4000, 0x4200, 0x4400, 0x4600, 0x4A00):
            sz = sz_from2((op >> 6) & 3)
            if not sz: raise Fallback()
            e = self.ea_get(I, st, mode, reg, sz); v = self.ea_rd(I, e, sz); m, sb = hx(M[sz]), hx(SB[sz]); src = self.regsrc(e)
            if h == 0x4000: I.c.append('u32 r=SUB(%s,0,%s,%s,(sr&FX_)?1u:0u,1,1);' % (v, m, sb)); self.ea_wr(I, e, sz, 'r', src)
            elif h == 0x4200:
                if e[0] == 'D': I.wr.append(('d%d' % e[1], sz != 4, set()))
                self.ea_wr(I, e, sz, '0', ()) if e[0] != 'D' else I.c.append(('AD[%d]=0;' % e[1]) if sz == 4 else 'AD[%d]&=~%s;' % (e[1], m))
                I.c.append('sr=(sr&~(FN_|FV_|FC_))|FZ_;')
            elif h == 0x4400: I.c.append('u32 r=SUB(%s,0,%s,%s,0,1,0);' % (v, m, sb)); self.ea_wr(I, e, sz, 'r', src)
            elif h == 0x4600: I.c.append('u32 r=~%s;' % v); self.nz(I, 'r', sz); self.ea_wr(I, e, sz, 'r', src)
            else: self.nz(I, v, sz)
            return
        if (op & 0xFFF0) == 0x4E40: raise Fallback()      # TRAP
        if (op & 0xFFF8) == 0x4E50:                       # LINK
            d = s16(self.w16(st['pc'])); st['pc'] += 2
            self.push32(I, 'AA[%d]' % reg); I.rd.add('a%d' % reg); I.wr.append(('a%d' % reg, False, {'a7'}))
            I.c.append('AA[%d]=AA[7]; AA[7]+=%s;' % (reg, hx(d))); return
        if (op & 0xFFF8) == 0x4E58:                       # UNLK
            I.rd.add('a%d' % reg); I.wr.append(('a7', False, {'a%d' % reg})); I.wr.append(('a%d' % reg, False, {'a%d' % reg}))
            I.c.append('AA[7]=AA[%d]; { u32 v=R32(AA[7]); AA[7]+=4; AA[%d]=v; }' % (reg, reg)); return
        if op == 0x4E71: return                           # NOP
        if op == 0x4E75:                                  # RTS
            I.rd.add('a7'); I.wr.append(('a7', False, {'a7'})); I.fall = False
            I.c.append('{ u32 t=R32(AA[7]); AA[7]+=4; cyc+=8;'); I.cyc += 8; I.c.append('EXITV{t}'); I.c.append('}'); return
        if op == 0x4E77:                                  # RTR
            I.rd.add('a7'); I.wr.append(('a7', False, {'a7'})); I.fall = False
            I.c.append('{ u32 s=R16(AA[7]); AA[7]+=2; sr=(sr&0xFF00u)|(s&0x1Fu); u32 t=R32(AA[7]); AA[7]+=4;'); I.c.append('EXITV{t}'); I.c.append('}'); return
        raise Fallback()                                  # RESET STOP RTE TRAPV MOVE USP illegal

    def movem(self, I, st, op):
        dr, sz = (op >> 10) & 1, 4 if op & 0x40 else 2
        mode, reg = (op >> 3) & 7, op & 7
        mask = self.w16(st['pc']); st['pc'] += 2
        names = ['AD[%d]' % i for i in range(8)] + ['AA[%d]' % i for i in range(8)]
        rn = ['d%d' % i for i in range(8)] + ['a%d' % i for i in range(8)]
        if mode == 4:
            if dr: raise Fallback()
            I.rd.add('a%d' % reg); I.memw = True
            I.c.append('{ u32 a=AA[%d];' % reg)
            if sz == 4:
                I.c.append('u32 native_n=m68k_movem_ram(a,0x%Xu,2u); if(native_n){a-=4u*native_n;cyc+=8u*native_n;}else{' % mask)
            for i in range(15, -1, -1):
                if mask & (1 << (15 - i)):
                    I.rd.add(rn[i]); I.c.append('a-=%d; W%d(a,%s); cyc+=%d;' % (sz, sz * 8, names[i], 8 if sz == 4 else 4)); I.cyc += 8 if sz == 4 else 4
            if sz == 4: I.c.append('}')
            I.c.append('AA[%d]=a; }' % reg); I.wr.append(('a%d' % reg, False, {'a%d' % reg})); return
        if mode == 3:
            I.rd.add('a%d' % reg); I.c.append('{ u32 a=AA[%d];' % reg)
        else:
            if mode in (0, 1, 4) or (mode == 7 and reg == 4): raise Fallback()
            e = self.ea_get(I, st, mode, reg, sz)
            if e[0] != 'M': raise Fallback()
            I.c.append('{ u32 a=%s;' % e[1])
        if sz == 4:
            I.c.append('u32 native_n=m68k_movem_ram(a,0x%Xu,%du); if(native_n){a+=4u*native_n;cyc+=8u*native_n;}else{' % (mask, dr))
        for i in range(16):
            if mask & (1 << i):
                c = 8 if sz == 4 else 4; I.cyc += c
                if dr:
                    I.wr.append((rn[i], False, set()))
                    v = 'R32(a)' if sz == 4 else '(u32)(s32)(s16)R16(a)'
                    I.c.append('%s=%s; a+=%d; cyc+=%d;' % (names[i], v, sz, c))
                else:
                    I.rd.add(rn[i]); I.memw = True
                    I.c.append('W%d(a,%s); a+=%d; cyc+=%d;' % (sz * 8, names[i], sz, c))
        if sz == 4: I.c.append('}')
        if mode == 3:
            I.c.append('AA[%d]=a;' % reg); I.wr.append(('a%d' % reg, False, {'a%d' % reg}))
        I.c.append('}')

    def line5(self, I, st, op):
        mode, reg = (op >> 3) & 7, op & 7
        if ((op >> 6) & 3) == 3:
            c = (op >> 8) & 15
            if mode == 1:                                  # DBcc
                disp = s16(self.w16(st['pc'])); st['pc'] += 2
                t = (st['pc'] - 2 + disp) & 0xFFFFFF
                I.rd.add('d%d' % reg); I.wr.append(('d%d' % reg, True, {'d%d' % reg})); I.succ.append(t)
                I.cyc = None; I.cmaxv = 4
                I.c.append('if(!%s){ u32 w=(AD[%d]-1u)&0xFFFFu; AD[%d]=(AD[%d]&0xFFFF0000u)|w; if(w!=0xFFFFu){ JUMP{%X} } }' % (self.cond(c), reg, reg, reg, t))
                return
            e = self.ea_get(I, st, mode, reg, 1)
            self.ea_wr(I, e, 1, '(%s?0xFFu:0u)' % self.cond(c), ()); return
        sz = sz_from2((op >> 6) & 3); q = (op >> 9) & 7 or 8
        if mode == 1:
            I.rd.add('a%d' % reg); I.wr.append(('a%d' % reg, False, {'a%d' % reg}))
            I.c.append('AA[%d]%s=%du;' % (reg, '-' if op & 0x100 else '+', q)); return
        if not sz: raise Fallback()
        e = self.ea_get(I, st, mode, reg, sz); v = self.ea_rd(I, e, sz); m, sb = hx(M[sz]), hx(SB[sz])
        if op & 0x100: I.c.append('u32 r=SUB(%du,%s,%s,%s,0,1,0);' % (q, v, m, sb))
        else: I.c.append('u32 r=ADD(%du,%s,%s,%s,%d,0,1,0);' % (q, v, m, sb, BITS[sz]))
        self.ea_wr(I, e, sz, 'r', self.regsrc(e))

    def line6(self, I, st, op):
        c = (op >> 8) & 15; disp = s8(op & 0xFF); base = st['pc']
        if disp == 0: disp = s16(self.w16(st['pc'])); st['pc'] += 2
        t = (base + disp) & 0xFFFFFF
        if c == 1:                                        # BSR
            self.push32(I, '(hi|%s)' % hx(st['pc'])); I.c.append('cyc+=8;'); I.cyc += 8; I.fall = False; I.call = t
            I.c.append('JUMP{%X}' % t); return
        I.succ.append(t)
        if c == 0:
            I.fall = False; I.c.append('cyc+=2;'); I.cyc += 2; I.c.append('FOLD{%X}' % t); I.c.append('JUMP{%X}' % t); return
        I.cyc = None                                      # taken/not taken differ; fold analysis uses ntcyc
        I.ntcyc = 4; I.cmaxv = 6
        I.c.append('if(%s){ cyc+=2; FOLD{%X} JUMP{%X} }' % (self.cond(c), t, t))

    def line8(self, I, st, op):
        dr, mode, reg, opm = (op >> 9) & 7, (op >> 3) & 7, op & 7, (op >> 6) & 7
        if opm in (3, 7):
            e = self.ea_get(I, st, mode, reg, 2); s = self.ea_rd(I, e, 2)
            I.c.append('u32 dv=(%s)&0xFFFFu; cyc+=120;' % s); I.cyc += 120
            I.c.append('if(!dv){ EXC{5} }')
            I.rd.add('d%d' % dr); I.wr.append(('d%d' % dr, False, {'d%d' % dr} | self.regsrc(e)))
            if opm == 3:
                I.c.append('{ u32 q=AD[%d]/dv, rm=AD[%d]%%dv; if(q>0xFFFFu){ sr|=FV_; sr&=~FC_; } else { AD[%d]=(rm<<16)|q; NZ(q,0xFFFFu,0x8000u); } }' % (dr, dr, dr))
            else:
                I.c.append('{ s32 a=(s32)AD[%d], b=(s32)(s16)dv; if(a==(s32)0x80000000&&b==-1){ sr|=FV_; } else { s32 q=a/b, rm=a%%b; if(q>32767||q<-32768){ sr|=FV_; sr&=~FC_; } else { AD[%d]=((u32)(rm&0xFFFF)<<16)|((u32)q&0xFFFFu); NZ((u32)q,0xFFFFu,0x8000u); } } }' % (dr, dr))
            return
        if (op & 0x1F0) == 0x100:                         # SBCD
            if op & 8:
                I.memw = True; I.anmod = True
                I.c.append('AA[%d]-=%du; AA[%d]-=%du; { u32 s=R8(AA[%d]), d=R8(AA[%d]); W8(AA[%d],f_bcd_sub(&sr,s,d)); }' % (reg, 2 if reg == 7 else 1, dr, 2 if dr == 7 else 1, reg, dr, dr))
            else:
                I.rd |= {'d%d' % reg, 'd%d' % dr}; I.wr.append(('d%d' % dr, True, {'d%d' % reg, 'd%d' % dr}))
                I.c.append('{ u32 r=f_bcd_sub(&sr,AD[%d]&0xFFu,AD[%d]&0xFFu); AD[%d]=(AD[%d]&~0xFFu)|r; }' % (reg, dr, dr, dr))
            return
        sz = sz_from2(opm & 3)
        if not sz: raise Fallback()
        e = self.ea_get(I, st, mode, reg, sz); s = self.ea_rd(I, e, sz); I.rd.add('d%d' % dr)
        I.c.append('u32 r=(AD[%d]|%s)&%s;' % (dr, s, hx(M[sz]))); self.nz(I, 'r', sz)
        if opm < 3:
            I.wr.append(('d%d' % dr, sz != 4, {'d%d' % dr} | self.regsrc(e)))
            I.c.append('AD[%d]=(AD[%d]&~%s)|r;' % (dr, dr, hx(M[sz])))
        else: self.ea_wr(I, e, sz, 'r', {'d%d' % dr} | self.regsrc(e))

    def addsub(self, I, st, op, sub):
        dr, mode, reg, opm = (op >> 9) & 7, (op >> 3) & 7, op & 7, (op >> 6) & 7
        if opm in (3, 7):
            sz = 2 if opm == 3 else 4
            e = self.ea_get(I, st, mode, reg, sz); s = self.ea_rd(I, e, sz)
            if sz == 2: s = '(u32)(s32)(s16)%s' % s
            I.rd.add('a%d' % dr); I.wr.append(('a%d' % dr, False, {'a%d' % dr} | self.regsrc(e)))
            I.c.append('AA[%d]%s=%s;' % (dr, '-' if sub else '+', s)); return
        sz = sz_from2(opm & 3)
        if not sz: raise Fallback()
        m, sb = hx(M[sz]), hx(SB[sz])
        if opm >= 4 and mode in (0, 1):                   # ADDX/SUBX
            fn = 'SUB(%s,%s,%s,%s,(sr&FX_)?1u:0u,1,1)' if sub else 'ADD(%s,%s,%s,%s,' + str(BITS[sz]) + ',(sr&FX_)?1u:0u,1,1)'
            if mode == 0:
                I.rd |= {'d%d' % reg, 'd%d' % dr}; I.wr.append(('d%d' % dr, sz != 4, {'d%d' % reg, 'd%d' % dr}))
                I.c.append('{ u32 r=%s; AD[%d]=(AD[%d]&~%s)|r; }' % (fn % ('AD[%d]' % reg, 'AD[%d]' % dr, m, sb), dr, dr, m))
            else:
                I.memw = True; I.anmod = True
                stp = 2 if (sz == 1 and reg == 7) else sz; dtp = 2 if (sz == 1 and dr == 7) else sz
                I.c.append('{ AA[%d]-=%d; u32 s=R%d(AA[%d]); AA[%d]-=%d; u32 d=R%d(AA[%d]); u32 r=%s; W%d(AA[%d],r); }' %
                           (reg, stp, sz * 8, reg, dr, dtp, sz * 8, dr, fn % ('s', 'd', m, sb), sz * 8, dr))
            return
        e = self.ea_get(I, st, mode, reg, sz)
        if opm < 3:
            s = self.ea_rd(I, e, sz); I.rd.add('d%d' % dr)
            r = ('SUB(%s,AD[%d],%s,%s,0,1,0)' % (s, dr, m, sb)) if sub else ('ADD(%s,AD[%d],%s,%s,%d,0,1,0)' % (s, dr, m, sb, BITS[sz]))
            I.wr.append(('d%d' % dr, sz != 4, {'d%d' % dr} | self.regsrc(e)))
            I.c.append('{ u32 r=%s; AD[%d]=(AD[%d]&~%s)|r; }' % (r, dr, dr, m))
        else:
            d = self.ea_rd(I, e, sz); I.rd.add('d%d' % dr)
            r = ('SUB(AD[%d],%s,%s,%s,0,1,0)' % (dr, d, m, sb)) if sub else ('ADD(AD[%d],%s,%s,%s,%d,0,1,0)' % (dr, d, m, sb, BITS[sz]))
            I.c.append('u32 r=%s;' % r); self.ea_wr(I, e, sz, 'r', {'d%d' % dr})

    def lineB(self, I, st, op):
        dr, mode, reg, opm = (op >> 9) & 7, (op >> 3) & 7, op & 7, (op >> 6) & 7
        if opm in (3, 7):
            sz = 2 if opm == 3 else 4
            e = self.ea_get(I, st, mode, reg, sz); s = self.ea_rd(I, e, sz)
            if sz == 2: s = '(u32)(s32)(s16)%s' % s
            I.rd.add('a%d' % dr); I.c.append('CMP(%s,AA[%d],0xFFFFFFFFu,0x80000000u);' % (s, dr)); return
        sz = sz_from2(opm & 3)
        if not sz: raise Fallback()
        m, sb = hx(M[sz]), hx(SB[sz])
        if opm < 3:
            e = self.ea_get(I, st, mode, reg, sz); s = self.ea_rd(I, e, sz); I.rd.add('d%d' % dr)
            I.c.append('CMP(%s,AD[%d],%s,%s);' % (s, dr, m, sb)); return
        if mode == 1:                                     # CMPM
            stp = 2 if (sz == 1 and reg == 7) else sz; dtp = 2 if (sz == 1 and dr == 7) else sz
            I.anmod = True; I.rd |= {'a%d' % reg, 'a%d' % dr}
            I.c.append('{ u32 s=R%d(AA[%d]); AA[%d]+=%d; u32 d=R%d(AA[%d]); AA[%d]+=%d; CMP(s,d,%s,%s); }' % (sz * 8, reg, reg, stp, sz * 8, dr, dr, dtp, m, sb))
            return
        e = self.ea_get(I, st, mode, reg, sz); v = self.ea_rd(I, e, sz); I.rd.add('d%d' % dr)
        I.c.append('u32 r=(%s^AD[%d])&%s;' % (v, dr, m)); self.nz(I, 'r', sz); self.ea_wr(I, e, sz, 'r', {'d%d' % dr} | self.regsrc(e))

    def lineC(self, I, st, op):
        dr, mode, reg, opm = (op >> 9) & 7, (op >> 3) & 7, op & 7, (op >> 6) & 7
        if opm in (3, 7):
            e = self.ea_get(I, st, mode, reg, 2); s = self.ea_rd(I, e, 2); I.rd.add('d%d' % dr)
            if opm == 3: I.c.append('{ u32 r=(AD[%d]&0xFFFFu)*((%s)&0xFFFFu);' % (dr, s))
            else: I.c.append('{ u32 r=(u32)((s32)(s16)AD[%d]*(s32)(s16)(%s));' % (dr, s))
            I.c.append('AD[%d]=r; NZ(r,0xFFFFFFFFu,0x80000000u); cyc+=50; }' % dr); I.cyc += 50
            I.wr.append(('d%d' % dr, False, {'d%d' % dr} | self.regsrc(e))); return
        if (op & 0x1F0) == 0x100:                         # ABCD
            if op & 8:
                I.memw = True; I.anmod = True
                I.c.append('AA[%d]-=%du; AA[%d]-=%du; { u32 s=R8(AA[%d]), d=R8(AA[%d]); W8(AA[%d],f_bcd_add(&sr,s,d)); }' % (reg, 2 if reg == 7 else 1, dr, 2 if dr == 7 else 1, reg, dr, dr))
            else:
                I.rd |= {'d%d' % reg, 'd%d' % dr}; I.wr.append(('d%d' % dr, True, {'d%d' % reg, 'd%d' % dr}))
                I.c.append('{ u32 r=f_bcd_add(&sr,AD[%d]&0xFFu,AD[%d]&0xFFu); AD[%d]=(AD[%d]&~0xFFu)|r; }' % (reg, dr, dr, dr))
            return
        if (op & 0x1F8) == 0x140:
            I.rd |= {'d%d' % dr, 'd%d' % reg}; I.wr += [('d%d' % dr, False, {'d%d' % reg}), ('d%d' % reg, False, {'d%d' % dr})]
            I.c.append('{ u32 t=AD[%d]; AD[%d]=AD[%d]; AD[%d]=t; }' % (dr, dr, reg, reg)); return
        if (op & 0x1F8) == 0x148:
            I.rd |= {'a%d' % dr, 'a%d' % reg}; I.wr += [('a%d' % dr, False, {'a%d' % reg}), ('a%d' % reg, False, {'a%d' % dr})]
            I.c.append('{ u32 t=AA[%d]; AA[%d]=AA[%d]; AA[%d]=t; }' % (dr, dr, reg, reg)); return
        if (op & 0x1F8) == 0x188:
            I.rd |= {'d%d' % dr, 'a%d' % reg}; I.wr += [('d%d' % dr, False, {'a%d' % reg}), ('a%d' % reg, False, {'d%d' % dr})]
            I.c.append('{ u32 t=AD[%d]; AD[%d]=AA[%d]; AA[%d]=t; }' % (dr, dr, reg, reg)); return
        sz = sz_from2(opm & 3)
        if not sz: raise Fallback()
        e = self.ea_get(I, st, mode, reg, sz); s = self.ea_rd(I, e, sz); I.rd.add('d%d' % dr)
        I.c.append('u32 r=(AD[%d]&%s)&%s;' % (dr, s, hx(M[sz]))); self.nz(I, 'r', sz)
        if opm < 3:
            I.wr.append(('d%d' % dr, sz != 4, {'d%d' % dr} | self.regsrc(e)))
            I.c.append('AD[%d]=(AD[%d]&~%s)|r;' % (dr, dr, hx(M[sz])))
        else: self.ea_wr(I, e, sz, 'r', {'d%d' % dr} | self.regsrc(e))

    def lineE(self, I, st, op):
        if ((op >> 6) & 3) == 3:
            tp, left = (op >> 9) & 3, (op >> 8) & 1
            e = self.ea_get(I, st, (op >> 3) & 7, op & 7, 2); v = self.ea_rd(I, e, 2)
            I.c.append('u32 r=f_shift(&sr,&cyc,%d,%d,%s,1,0xFFFFu,0x8000u);' % (tp, left, v)); I.cyc += 2
            self.ea_wr(I, e, 2, 'r', self.regsrc(e)); return
        sz = sz_from2((op >> 6) & 3)
        if not sz: raise Fallback()
        left, tp, reg, cnt = (op >> 8) & 1, (op >> 3) & 3, op & 7, (op >> 9) & 7
        I.rd.add('d%d' % reg); src = {'d%d' % reg}
        if op & 0x20:
            I.rd.add('d%d' % cnt); src.add('d%d' % cnt); ce = '(int)(AD[%d]&63u)' % cnt; I.cmaxv = I.cyc + 126; I.cyc = None
        else:
            cnt = cnt or 8; ce = '%d' % cnt; I.cyc += 2 * cnt
        I.wr.append(('d%d' % reg, sz != 4, src))
        I.c.append('{ u32 r=f_shift(&sr,&cyc,%d,%d,AD[%d],%s,%s,%s); AD[%d]=(AD[%d]&~%s)|(r&%s); }' % (tp, left, reg, ce, hx(M[sz]), hx(SB[sz]), reg, reg, hx(M[sz]), hx(M[sz])))
        if I.cyc is not None and tp == 2: pass


# ---------------------------------------------------------------------- regions and C output
def decode_all(tx, addrs):
    insns = {}
    for a in sorted(addrs):
        insns[a] = tx.insn(a)
    # drop instructions overlapping an earlier one (a decode that started inside another instruction)
    keep = {}; last_end = -1
    for a in sorted(insns):
        if a < last_end: continue
        keep[a] = insns[a]; last_end = a + insns[a].length
    return keep


def regions_of(insns):
    addrs = sorted(insns)
    parent = {a: a for a in addrs}
    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x
    for a in addrs:
        for t in insns[a].succ:
            if t in insns: parent[find(a)] = find(t)
    regions = {}
    for a in addrs: regions.setdefault(find(a), []).append(a)
    return sorted((sorted(m) for m in regions.values()), key=lambda m: m[0])


COND_USE = {0: 0, 1: 0, 2: 5, 3: 5, 4: 1, 5: 1, 6: 4, 7: 4, 8: 2, 9: 2, 10: 8, 11: 8, 12: 10, 13: 10, 14: 14, 15: 14}
ALL = 31


def flag_ud(I):
    """(use, def) masks of the CCR bits (X N Z V C = 16 8 4 2 1) of one translated instruction, from its C text"""
    if I.interp: return ALL, 0
    t = '\n'.join(I.c); u = 0; d = 0
    for m in re.finditer(r'CC\(sr,(\d+)\)', t): u |= COND_USE[int(m.group(1))]
    if 'sr&FX_' in t: u |= 16
    if re.search(r'(ADD|SUB)\([^;]*,1\)', t): u |= 4                          # ADDX/SUBX/NEGX: Z only cleared
    if 'f_bcd' in t: u |= 16 | 4
    if 'sr&0xFFFFu' in t or 'sr|=0x' in t or 'sr&=0x' in t or 'sr^=0x' in t or 'FV_; sr&=~FC_' in t or 'sr|=FV_' in t:
        return ALL, 0                                                          # CCR/SR access, DIV: everything live
    if 'f_shift(&sr,&cyc,2,' in t: u |= 16
    if 'NZ(' in t or 'CMP(' in t or 'sr=(sr&~(FN_|FV_|FC_))|FZ_' in t: d |= 15
    for m in re.finditer(r'(ADD|SUB)\(([^;]*)\)', t):
        a = m.group(2).split(',')
        d |= 11 | (16 if a[-2].strip() == '1' else 0) | (0 if a[-1].strip() == '1' else 4)
    if 'f_shift(' in t:
        d |= 15 | (0 if 'f_shift(&sr,&cyc,3,' in t else 16)
        if re.search(r'f_shift\(&sr,&cyc,\d,\d,AD\[\d\],\(int\)', t): u |= 16    # count may be 0: X/C read
    if 'sr=(sr&0xFF00u)|' in t: d |= 31
    if 'sr&=~FZ_; else sr|=FZ_' in t: d |= 4
    if 'f_bcd' in t: d |= 1 | 16
    return u, d


def flag_liveness(insns, members):
    mset = set(members)
    ud = {a: flag_ud(insns[a]) for a in members}
    succ = {}
    for a in members:
        I = insns[a]; ss = []; out = False
        if I.interp: out = True
        for t in I.succ:
            if t in mset: ss.append(t)
            else: out = True
        if I.call is not None or not I.fall and not I.succ: out = True
        txt = '\n'.join(I.c)
        if 'EXITV' in txt or 'EXC{' in txt or 'JUMPA' in txt: out = True
        succ[a] = (ss, out)
    live_in = {a: ALL for a in members}
    changed = True
    while changed:
        changed = False
        for a in reversed(members):
            ss, out = succ[a]
            lo = ALL if out else 0
            for t in ss: lo |= live_in[t]
            u, d = ud[a]
            li = u | (lo & ~d)
            if li != live_in[a]: live_in[a] = li; changed = True
    live_out = {}
    for a in members:
        ss, out = succ[a]; lo = ALL if out else 0
        for t in ss: lo |= live_in[t]
        live_out[a] = lo
    return live_out


def apply_live(code, L):
    code = code.replace('NZ(', 'NZL(%du,' % L)
    code = re.sub(r'\bADD\(', 'ADDL(%du,' % L, code)
    code = re.sub(r'\bSUB\(', 'SUBL(%du,' % L, code)
    code = re.sub(r'\bCMP\(', 'CMPL(%du,' % L, code)
    return code


def emit_region(insns, members, fname):
    """C text of one region function (name placeholder @F@ for dedup) and its entry list"""
    mset = set(members)
    idx = {a: i for i, a in enumerate(members)}
    folds = []
    body = []
    exits = {}                                     # exit pc -> stub index
    def exit_to(pc):
        if pc not in exits: exits[pc] = len(exits)
        return 'i=%d; goto ex;' % exits[pc]
    live = flag_liveness(insns, members)
    for i, a in enumerate(members):
        I = insns[a]
        body.append('B%X: if((s32)(cyc-lim)>=0){ %s }' % (a, exit_to(a)))
        if I.interp:
            body.append('rc=AOT_INTERP; %s' % exit_to(a)); continue
        code = apply_live('\n'.join(I.c), live[a])
        def jump(m):
            t = int(m.group(1), 16)
            if t in mset: return 'goto B%X;' % t
            return exit_to(t)
        def jumpa(m):
            t = int(m.group(1), 16)                    # absolute target: literal value, keeps its own high byte
            if (t & 0xFFFFFF) in mset: return 'if(hi==0x%Xu) goto B%X; npc=0x%Xu; goto out;' % (t & 0xFF000000, t & 0xFFFFFF, t)
            return 'npc=0x%Xu; goto out;' % t
        def fold(m, a=a):
            t = int(m.group(1), 16)
            f = fold_info(insns, t, a)
            if f is None: return ''
            per, idle = f; k = len(folds); folds.append(k)
            return ('if(fx%d==C.fx&&C.spin_enable){ u32 rem=lim-cyc; if((s32)rem>=%d){ cyc+=(rem/%du)*%du; C.spin_skips++; C.idle_ok=(lrd%d==C.lrd); } } '
                    'fx%d=C.fx; lrd%d=C.lrd; ' % (k, per, per, per, k, k, k))
        code = re.sub(r'FOLD\{(\w+)\}', fold, code)
        code = re.sub(r'JUMPA\{(\w+)\}', jumpa, code)
        code = re.sub(r'JUMP\{(\w+)\}', jump, code)
        code = re.sub(r'EXITV\{(\w+)\}', r'npc=\1; goto out;', code)
        code = re.sub(r'EXC\{(\w+)\}', lambda m: 'C.pc=hi|0x%Xu; C.sr=(u16)sr; C.cycles=cyc; m68k_exception(%s); npc=C.pc; sr=C.sr; cyc=C.cycles; goto out;' % (a + I.length, m.group(1)), code)
        body.append('{ %s }' % code)
        if I.fall:
            nxt = a + I.length
            if i + 1 < len(members) and members[i + 1] == nxt: pass
            elif nxt in mset: body.append('goto B%X;' % nxt)
            else: body.append(exit_to(nxt))
    decl = ''.join('u32 fx%d=0xFFFFFFFFu, lrd%d=0;' % (k, k) for k in folds)
    sw = ' '.join('case %d: goto B%X;' % (i, a) for i, a in enumerate(members))
    xt = ','.join('0x%Xu' % pc for pc, _ in sorted(exits.items(), key=lambda kv: kv[1]))
    text = ('u32 @F@(u32 entry){\n static const u32 xpc[] AOT_TAB={%s};\n u32 sr=C.sr, cyc=C.cycles, npc, i, rc=AOT_CONT; const u32 lim=aot_lim, hi=C.pc&0xFF000000u; %s\n switch(entry){ %s default: npc=C.pc; goto out; }\n'
            % (xt, decl, sw)) + '\n'.join(body) + '\nex: npc=hi|xpc[i];\nout: C.sr=(u16)sr; C.cycles=cyc; C.pc=npc; return rc;\n}'
    return text, [(a, idx[a]) for a in members]


def fold_info(insns, t, b):
    """Loop from t to the backward branch at b: (cycles per iteration, idle_ok) if whole iterations can be
    skipped, else None.  Requires a straight-line body (forward exits allowed), constant cycles, no memory
    writes, no (An)+/-(An), and registers written in the body to be recomputed each iteration from values that
    do not change in the loop."""
    if t > b: return None
    body = sorted(a for a in insns if t <= a <= b)
    a = t; per = 0; idle = 1
    for x in body:
        if x != a: return None
        I = insns[x]
        if I.interp or I.memw or I.anmod or I.call is not None: return None
        if x == b:
            per += 6 if I.cyc is None else I.cyc    # taken Bcc: 4+2 ; BRA: constant
        else:
            c = I.cyc if I.cyc is not None else getattr(I, 'ntcyc', None)
            if c is None: return None
            for s in I.succ:
                if s != x + I.length and t <= s <= b: return None   # internal branch
            per += c
        a = x + I.length
    # register determinism
    written = set()
    for x in body:
        for (r, partial, srcs) in insns[x].wr: written.add(r)
    stable = set()
    for x in body:
        I = insns[x]
        for r in I.rd:
            if r in written and r not in stable: return None
        for (r, partial, srcs) in I.wr:
            if any(s in written and s not in stable and s != r for s in srcs): return None
            if r in srcs and r not in stable: return None
            stable.add(r)
    return per, idle


if __name__ == "__main__":
    print("use aotgen.py")
