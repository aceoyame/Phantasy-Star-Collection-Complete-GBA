#!/usr/bin/env python3
"""Minimal ARMv4T (ARM7TDMI) disassembler: ARM + Thumb. Written because the
sandbox has no ARM binutils/capstone. Covers the full ARMv4T encoding space
used by GBA code (no coprocessor detail)."""
import struct, sys

COND = ['eq','ne','cs','cc','mi','pl','vs','vc','hi','ls','ge','lt','gt','le','','nv']
REG = ['r0','r1','r2','r3','r4','r5','r6','r7','r8','r9','r10','r11','r12','sp','lr','pc']
DP = ['and','eor','sub','rsb','add','adc','sbc','rsc','tst','teq','cmp','cmn','orr','mov','bic','mvn']
SH = ['lsl','lsr','asr','ror']

IOREG = {
 0x4000000:'DISPCNT',0x4000004:'DISPSTAT',0x4000006:'VCOUNT',0x4000008:'BG0CNT',0x400000A:'BG1CNT',
 0x400000C:'BG2CNT',0x400000E:'BG3CNT',0x4000010:'BG0HOFS',0x4000040:'WIN0H',0x4000048:'WININ',0x400004A:'WINOUT',
 0x400004C:'MOSAIC',0x4000050:'BLDCNT',0x4000052:'BLDALPHA',0x4000054:'BLDY',
 0x4000060:'SOUND1CNT_L',0x4000080:'SOUNDCNT_L',0x4000082:'SOUNDCNT_H',0x4000084:'SOUNDCNT_X',0x4000088:'SOUNDBIAS',
 0x40000A0:'FIFO_A',0x40000A4:'FIFO_B',0x40000B0:'DMA0SAD',0x40000B4:'DMA0DAD',0x40000B8:'DMA0CNT_L',0x40000BA:'DMA0CNT_H',
 0x40000BC:'DMA1SAD',0x40000C0:'DMA1DAD',0x40000C4:'DMA1CNT_L',0x40000C6:'DMA1CNT_H',0x40000C8:'DMA2SAD',0x40000CC:'DMA2DAD',
 0x40000D0:'DMA2CNT_L',0x40000D2:'DMA2CNT_H',0x40000D4:'DMA3SAD',0x40000D8:'DMA3DAD',0x40000DC:'DMA3CNT_L',0x40000DE:'DMA3CNT_H',
 0x4000100:'TM0CNT_L',0x4000102:'TM0CNT_H',0x4000104:'TM1CNT_L',0x4000106:'TM1CNT_H',0x4000108:'TM2CNT_L',0x400010A:'TM2CNT_H',
 0x400010C:'TM3CNT_L',0x400010E:'TM3CNT_H',0x4000130:'KEYINPUT',0x4000132:'KEYCNT',0x4000134:'RCNT',
 0x4000200:'IE',0x4000202:'IF',0x4000204:'WAITCNT',0x4000208:'IME',0x4000300:'POSTFLG',0x4000301:'HALTCNT',0x4000800:'MEMCNT',
 0x3007FFC:'BIOS_IRQ_VEC',0x3007FF8:'BIOS_IRQ_ACK',0x3007FFA:'BIOS_SOFTRESET_FLAG',0x3007FF4:'BIOS_DMA_SOUND_ACK',
}

def rlist(m):
    return '{' + ','.join(REG[i] for i in range(16) if m >> i & 1) + '}'

def ror(v, r):
    r &= 31
    return ((v >> r) | (v << (32 - r))) & 0xFFFFFFFF if r else v

class Mem:
    def __init__(self, data, base):
        self.d, self.b = data, base
    def ok(self, a, n=4):
        o = a - self.b
        return 0 <= o and o + n <= len(self.d)
    def u32(self, a):
        return struct.unpack_from('<I', self.d, a - self.b)[0]
    def u16(self, a):
        return struct.unpack_from('<H', self.d, a - self.b)[0]

def note(v):
    return f' ; {IOREG[v]}' if v in IOREG else ''

def arm(mem, a):
    w = mem.u32(a)
    c = COND[w >> 28]
    pc = a + 8
    if (w & 0x0FFFFFF0) == 0x012FFF10:
        return f'bx{c} {REG[w&15]}'
    if (w & 0x0E000000) == 0x0A000000:
        off = w & 0xFFFFFF
        if off & 0x800000: off -= 0x1000000
        return f'b{"l" if w>>24&1 else ""}{c} 0x{(pc + off*4) & 0xFFFFFFFF:08X}'
    if (w & 0x0F000000) == 0x0F000000:
        return f'swi{c} 0x{w & 0xFFFFFF:X}'
    if (w & 0x0FC000F0) == 0x00000090:
        rd, rn, rs, rm = w>>16&15, w>>12&15, w>>8&15, w&15
        s = 's' if w>>20&1 else ''
        if w>>21&1: return f'mla{c}{s} {REG[rd]},{REG[rm]},{REG[rs]},{REG[rn]}'
        return f'mul{c}{s} {REG[rd]},{REG[rm]},{REG[rs]}'
    if (w & 0x0F8000F0) == 0x00800090:
        op = ['umull','umlal','smull','smlal'][w>>21&3]
        return f'{op}{c} {REG[w>>12&15]},{REG[w>>16&15]},{REG[w&15]},{REG[w>>8&15]}'
    if (w & 0x0FB00FF0) == 0x01000090:
        return f'swp{c}{"b" if w>>22&1 else ""} {REG[w>>12&15]},{REG[w&15]},[{REG[w>>16&15]}]'
    if (w & 0x0E000090) == 0x00000090 and (w & 0x60):
        P,U,I,W,L = w>>24&1, w>>23&1, w>>22&1, w>>21&1, w>>20&1
        rn, rd = w>>16&15, w>>12&15
        sh = w>>5&3
        op = ('ldr' if L else 'str') + {1:'h',2:'sb',3:'sh'}[sh]
        if I: off = f'#{"" if U else "-"}0x{((w>>4&0xF0)|(w&15)):X}'
        else: off = f'{"" if U else "-"}{REG[w&15]}'
        extra = ''
        if I and rn == 15 and P:
            t = pc + (((w>>4&0xF0)|(w&15)) * (1 if U else -1))
            extra = f' ; =[0x{t:08X}]'
        if P: return f'{op}{c} {REG[rd]},[{REG[rn]},{off}]{"!" if W else ""}{extra}'
        return f'{op}{c} {REG[rd]},[{REG[rn]}],{off}'
    if (w & 0x0FBF0FFF) == 0x010F0000:
        return f'mrs{c} {REG[w>>12&15]},{"spsr" if w>>22&1 else "cpsr"}'
    if (w & 0x0DB0F000) == 0x0120F000:
        fl = ''.join(ch for i,ch in enumerate('cxsf') if w>>(16+i)&1)
        psr = ('spsr' if w>>22&1 else 'cpsr') + '_' + fl
        if w>>25&1:
            v = ror(w&0xFF, (w>>8&15)*2)
            return f'msr{c} {psr},#0x{v:X}'
        return f'msr{c} {psr},{REG[w&15]}'
    if (w & 0x0C000000) == 0x00000000:
        op = w>>21&15; S = w>>20&1; rn, rd = w>>16&15, w>>12&15
        if w>>25&1:
            v = ror(w&0xFF, (w>>8&15)*2); op2 = f'#0x{v:X}'
            if rn == 15 and op in (2,4):
                t = pc - v if op == 2 else pc + v
                op2 += f' ; =0x{t:08X}'
        else:
            rm = w&15; st = w>>5&3
            if w>>4&1: op2 = f'{REG[rm]},{SH[st]} {REG[w>>8&15]}'
            else:
                amt = w>>7&31
                if amt == 0 and st == 0: op2 = REG[rm]
                elif amt == 0 and st == 3: op2 = f'{REG[rm]},rrx'
                else: op2 = f'{REG[rm]},{SH[st]} #{amt or 32}'
        mn = DP[op]
        if op in (8,9,10,11): return f'{mn}{c} {REG[rn]},{op2}'
        s = 's' if S else ''
        if op in (13,15): return f'{mn}{c}{s} {REG[rd]},{op2}'
        return f'{mn}{c}{s} {REG[rd]},{REG[rn]},{op2}'
    if (w & 0x0C000000) == 0x04000000:
        I,P,U,B,W,L = w>>25&1,w>>24&1,w>>23&1,w>>22&1,w>>21&1,w>>20&1
        rn, rd = w>>16&15, w>>12&15
        op = ('ldr' if L else 'str') + ('b' if B else '')
        extra = ''
        if not I:
            off = f'#{"" if U else "-"}0x{w&0xFFF:X}'
            if rn == 15 and P:
                t = pc + ((w&0xFFF) if U else -(w&0xFFF))
                if mem.ok(t):
                    v = mem.u32(t); extra = f' ; =0x{v:08X}{note(v)}'
        else:
            amt = w>>7&31; st = w>>5&3
            off = f'{"" if U else "-"}{REG[w&15]}' + (f',{SH[st]} #{amt}' if amt else '')
        if P: return f'{op}{c} {REG[rd]},[{REG[rn]},{off}]{"!" if W else ""}{extra}'
        return f'{op}{c} {REG[rd]},[{REG[rn]}],{off}'
    if (w & 0x0E000000) == 0x08000000:
        P,U,S,W,L = w>>24&1,w>>23&1,w>>22&1,w>>21&1,w>>20&1
        mode = ('i' if U else 'd') + ('b' if P else 'a')
        return f'{"ldm" if L else "stm"}{mode}{c} {REG[w>>16&15]}{"!" if W else ""},{rlist(w&0xFFFF)}{"^" if S else ""}'
    return f'.word 0x{w:08X}'

def thumb(mem, a):
    h = mem.u16(a)
    pc = a + 4
    t = h >> 13
    if h >> 11 == 3:  # add/sub
        I, sub = h>>10&1, h>>9&1
        rn = h>>6&7
        return f'{"subs" if sub else "adds"} {REG[h&7]},{REG[h>>3&7]},{"#"+str(rn) if I else REG[rn]}'
    if t == 0:
        return f'{SH[h>>11&3]}s {REG[h&7]},{REG[h>>3&7]},#{h>>6&31}'
    if t == 1:
        return f'{["movs","cmp","adds","subs"][h>>11&3]} {REG[h>>8&7]},#0x{h&0xFF:X}'
    if h >> 10 == 0x10:
        ops = ['ands','eors','lsls','lsrs','asrs','adcs','sbcs','rors','tst','negs','cmp','cmn','orrs','muls','bics','mvns']
        return f'{ops[h>>6&15]} {REG[h&7]},{REG[h>>3&7]}'
    if h >> 10 == 0x11:
        op = h>>8&3; rd = (h&7) | (h>>4&8); rs = h>>3&15
        if op == 3: return f'bx {REG[rs]}'
        return f'{["add","cmp","mov"][op]} {REG[rd]},{REG[rs]}'
    if h >> 11 == 9:
        ta = (pc & ~3) + (h&0xFF)*4
        extra = ''
        if mem.ok(ta):
            v = mem.u32(ta); extra = f' ; =0x{v:08X}{note(v)}'
        return f'ldr {REG[h>>8&7]},[pc,#0x{(h&0xFF)*4:X}]{extra}'
    if h >> 12 == 5:
        ro = REG[h>>6&7]; rb = REG[h>>3&7]; rd = REG[h&7]
        if h>>9&1:
            op = ['strh','ldsb','ldrh','ldsh'][h>>10&3]
        else:
            op = ['str','strb','ldr','ldrb'][h>>10&3]
        return f'{op} {rd},[{rb},{ro}]'
    if t == 3:
        B, L = h>>12&1, h>>11&1
        off = (h>>6&31) * (1 if B else 4)
        return f'{"ldr" if L else "str"}{"b" if B else ""} {REG[h&7]},[{REG[h>>3&7]},#0x{off:X}]'
    if h >> 12 == 8:
        return f'{"ldrh" if h>>11&1 else "strh"} {REG[h&7]},[{REG[h>>3&7]},#0x{(h>>6&31)*2:X}]'
    if h >> 12 == 9:
        return f'{"ldr" if h>>11&1 else "str"} {REG[h>>8&7]},[sp,#0x{(h&0xFF)*4:X}]'
    if h >> 12 == 10:
        if h>>11&1: return f'add {REG[h>>8&7]},sp,#0x{(h&0xFF)*4:X}'
        return f'add {REG[h>>8&7]},pc,#0x{(h&0xFF)*4:X} ; =0x{(pc&~3)+(h&0xFF)*4:08X}'
    if h >> 8 == 0xB0:
        v = (h&0x7F)*4
        return f'{"sub" if h>>7&1 else "add"} sp,#0x{v:X}'
    if (h & 0xF600) == 0xB400:
        L, R = h>>11&1, h>>8&1
        m = h & 0xFF
        if R: m |= (1<<15) if L else (1<<14)
        return f'{"pop" if L else "push"} {rlist(m)}'
    if h >> 12 == 12:
        return f'{"ldmia" if h>>11&1 else "stmia"} {REG[h>>8&7]}!,{rlist(h&0xFF)}'
    if h >> 8 == 0xDF:
        return f'swi 0x{h&0xFF:X}'
    if h >> 12 == 13:
        cond = h>>8&15
        off = h & 0xFF
        if off & 0x80: off -= 0x100
        return f'b{COND[cond]} 0x{pc + off*2:08X}'
    if h >> 11 == 0x1C:
        off = h & 0x7FF
        if off & 0x400: off -= 0x800
        return f'b 0x{pc + off*2:08X}'
    if h >> 11 == 0x1E:
        if mem.ok(a+2, 2):
            h2 = mem.u16(a+2)
            if h2 >> 11 == 0x1F:
                hi = h & 0x7FF
                if hi & 0x400: hi -= 0x800
                tgt = pc + (hi << 12) + ((h2 & 0x7FF) << 1)
                return f'bl 0x{tgt & 0xFFFFFFFF:08X}', 4
        return f'bl.hi 0x{h&0x7FF:X}'
    if h >> 11 == 0x1F:
        return f'bl.lo 0x{h&0x7FF:X}'
    return f'.hword 0x{h:04X}'

def dis(mem, start, n, mode='arm', out=print):
    a = start
    for _ in range(n):
        if mode == 'arm':
            if not mem.ok(a): break
            out(f'{a:08X}: {mem.u32(a):08X}  {arm(mem, a)}')
            a += 4
        else:
            if not mem.ok(a, 2): break
            r = thumb(mem, a)
            if isinstance(r, tuple):
                txt, sz = r
                out(f'{a:08X}: {mem.u16(a):04X} {mem.u16(a+2):04X}  {txt}')
                a += sz
            else:
                out(f'{a:08X}: {mem.u16(a):04X}       {r}')
                a += 2
    return a

if __name__ == '__main__':
    path, base, start, n, mode = sys.argv[1], int(sys.argv[2], 0), int(sys.argv[3], 0), int(sys.argv[4]), sys.argv[5]
    data = open(path, 'rb').read()
    dis(Mem(data, base), start, n, mode)
