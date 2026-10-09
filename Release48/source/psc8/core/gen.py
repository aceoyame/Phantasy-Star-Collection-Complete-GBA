#!/usr/bin/env python3
"""Generates z80f_ops.inc: the opcode switch bodies of PSC8's fast Z80 core (z80f.h)."""
R = ['B','C','D','E','H','L','(HL)','A']
out = []
def emit(s): out.append(s)

def getr(i, ix=None):
    n = R[i]
    if ix:
        if n == 'H': return f'({ix}>>8)'
        if n == 'L': return f'({ix}&255)'
    return n
def setr(i, v, ix=None):
    n = R[i]
    if ix:
        if n == 'H': return f'{ix}=({ix}&255)|(({v})<<8);'
        if n == 'L': return f'{ix}=({ix}&0xff00)|({v});'
    return f'{n}=({v});'

ALU = ['ADD','ADC','SUB','SBC','AND','XOR','OR','CP']
def alu(k, v):
    return {'ADD':f'OP_ADD({v},0);','ADC':f'OP_ADD({v},F&1);','SUB':f'OP_SUB({v},0);','SBC':f'OP_SUB({v},F&1);',
            'AND':f'A&=({v});F=SZP[A]|HF;','XOR':f'A^=({v});F=SZP[A];','OR':f'A|=({v});F=SZP[A];','CP':f'OP_CP({v});'}[k]

COND = ['!(F&ZF)','(F&ZF)','!(F&CF)','(F&CF)','!(F&PF)','(F&PF)','!(F&SF)','(F&SF)']
PAIRS = ['BC','DE','HL','SP']

def main_ops(ix=None):
    """ix=None: unprefixed. ix='IX'/'IY': DD/FD table (only opcodes that differ are emitted; the rest fall to plain)."""
    HLn = ix if ix else 'HL'
    def hl_get(): return ix if ix else '((H<<8)|L)'
    def hl_set(v): return f'{ix}=({v})&0xffff;' if ix else f'{{uint32_t _t=({v});H=(_t>>8)&255;L=_t&255;}}'
    # effective address for (HL)/(IX+d)
    def ea(): return 'EA_IX' if ix else '((H<<8)|L)'
    pre = (lambda c: f'case 0x{c:02X}:') 
    def pair_get(p):
        if p=='HL': return hl_get()
        if p=='SP': return 'SP'
        return f'(({p[0]}<<8)|{p[1]})'
    def pair_set(p, v):
        if p=='HL': return hl_set(v)
        if p=='SP': return f'SP=({v})&0xffff;'
        return f'{{uint32_t _t=({v});{p[0]}=(_t>>8)&255;{p[1]}=_t&255;}}'
    ops = {}
    for i,p in enumerate(PAIRS):
        if ix and p!='HL':
            # ADD IX,pp
            ops[0x09+16*i] = f'{{uint32_t v={pair_get(p)};OP_ADD16({ix},v);}}'
            continue
        if not ix:
            ops[0x01+16*i] = f'{{uint32_t v=FETCH16();{pair_set(p,"v")}}}'
            ops[0x03+16*i] = pair_set(p, f'{pair_get(p)}+1')
            ops[0x0B+16*i] = pair_set(p, f'{pair_get(p)}-1')
            ops[0x09+16*i] = f'{{uint32_t v={pair_get(p)};uint32_t h=(H<<8)|L;OP_ADD16(h,v);H=h>>8;L=h&255;}}'
    if ix:
        ops[0x21] = f'{ix}=FETCH16();'
        ops[0x23] = f'{ix}=({ix}+1)&0xffff;'
        ops[0x2B] = f'{ix}=({ix}-1)&0xffff;'
        ops[0x29] = f'{{uint32_t v={ix};OP_ADD16({ix},v);}}'
        ops[0x22] = f'{{uint32_t a=FETCH16();WR8(a,{ix}&255);WR8((a+1)&0xffff,{ix}>>8);}}'
        ops[0x2A] = f'{{uint32_t a=FETCH16();{ix}=RD8(a)|(RD8((a+1)&0xffff)<<8);}}'
        ops[0xE1] = f'{ix}=POP16();'
        ops[0xE5] = f'PUSH16({ix});'
        ops[0xE3] = f'{{uint32_t v=RD8(SP)|(RD8((SP+1)&0xffff)<<8);WR8(SP,{ix}&255);WR8((SP+1)&0xffff,{ix}>>8);{ix}=v;}}'
        ops[0xE9] = f'PC={ix};'
        ops[0xF9] = f'SP={ix};'
        ops[0x34] = f'{{uint32_t a=EA_IX;uint32_t v=(RD8(a)+1)&255;F=(F&CF)|SZHV_INC[v];WR8(a,v);}}'
        ops[0x35] = f'{{uint32_t a=EA_IX;uint32_t v=(RD8(a)-1)&255;F=(F&CF)|SZHV_DEC[v];WR8(a,v);}}'
        ops[0x36] = f'{{uint32_t a=EA_IX;uint32_t n=FETCH();WR8(a,n);}}'
    else:
        ops[0x22] = '{uint32_t a=FETCH16();WR8(a,L);WR8((a+1)&0xffff,H);}'
        ops[0x2A] = '{uint32_t a=FETCH16();L=RD8(a);H=RD8((a+1)&0xffff);}'
        ops[0x34] = '{uint32_t a=(H<<8)|L;uint32_t v=(RD8(a)+1)&255;F=(F&CF)|SZHV_INC[v];WR8(a,v);}'
        ops[0x35] = '{uint32_t a=(H<<8)|L;uint32_t v=(RD8(a)-1)&255;F=(F&CF)|SZHV_DEC[v];WR8(a,v);}'
        ops[0x36] = '{uint32_t n=FETCH();WR8((H<<8)|L,n);}'
        ops[0xE1] = '{uint32_t v=POP16();H=v>>8;L=v&255;}'
        ops[0xE5] = 'PUSH16((H<<8)|L);'
        ops[0xE3] = '{uint32_t v=RD8(SP)|(RD8((SP+1)&0xffff)<<8);WR8(SP,L);WR8((SP+1)&0xffff,H);H=v>>8;L=v&255;}'
        ops[0xE9] = 'PC=(H<<8)|L;'
        ops[0xF9] = 'SP=(H<<8)|L;'
    # INC/DEC/LD r,n for 8-bit regs
    for i in range(8):
        if i == 6: continue
        n = R[i]
        if ix and n not in 'HL': continue
        g = getr(i, ix)
        ops[0x04+8*i] = f'{{uint32_t v=({g}+1)&255;F=(F&CF)|SZHV_INC[v];{setr(i,"v",ix)}}}'
        ops[0x05+8*i] = f'{{uint32_t v=({g}-1)&255;F=(F&CF)|SZHV_DEC[v];{setr(i,"v",ix)}}}'
        ops[0x06+8*i] = f'{{uint32_t v=FETCH();{setr(i,"v",ix)}}}'
    # LD r,r'
    for d in range(8):
        for s in range(8):
            op = 0x40+8*d+s
            if op == 0x76: continue
            if ix:
                if d == 6 or s == 6:
                    # LD r,(IX+d) / LD (IX+d),r : r uses plain H/L
                    if d == 6: ops[op] = f'WR8(EA_IX,{R[s]});'
                    else: ops[op] = f'{R[d]}=RD8(EA_IX);'
                elif R[d] in 'HL' or R[s] in 'HL':
                    ops[op] = setr(d, getr(s, ix), ix)
                continue
            if d == 6: ops[op] = f'WR8((H<<8)|L,{R[s]});'
            elif s == 6: ops[op] = f'{R[d]}=RD8((H<<8)|L);'
            elif d != s: ops[op] = f'{R[d]}={R[s]};'
            else: ops[op] = ''
    # ALU
    for k in range(8):
        for s in range(8):
            op = 0x80+8*k+s
            if ix:
                if s == 6: ops[op] = f'{{uint32_t v=RD8(EA_IX);{alu(ALU[k],"v")}}}'
                elif R[s] in 'HL': ops[op] = f'{{uint32_t v={getr(s,ix)};{alu(ALU[k],"v")}}}'
                continue
            if s == 6: ops[op] = f'{{uint32_t v=RD8((H<<8)|L);{alu(ALU[k],"v")}}}'
            else: ops[op] = f'{{uint32_t v={R[s]};{alu(ALU[k],"v")}}}'
    if ix: return ops
    # remaining unprefixed
    ops[0x00] = ''
    ops[0x02] = 'WR8((B<<8)|C,A);'
    ops[0x12] = 'WR8((D<<8)|E,A);'
    ops[0x0A] = 'A=RD8((B<<8)|C);'
    ops[0x1A] = 'A=RD8((D<<8)|E);'
    ops[0x32] = 'WR8(FETCH16(),A);'
    ops[0x3A] = 'A=RD8(FETCH16());'
    ops[0x07] = 'A=((A<<1)|(A>>7))&255;F=(F&(SF|ZF|PF))|(A&(YF|XF|CF));'
    ops[0x0F] = 'F=(F&(SF|ZF|PF))|(A&CF);A=((A>>1)|(A<<7))&255;F|=A&(YF|XF);'
    ops[0x17] = '{uint32_t c=A>>7;A=((A<<1)|(F&CF))&255;F=(F&(SF|ZF|PF))|(A&(YF|XF))|c;}'
    ops[0x1F] = '{uint32_t c=A&1;A=(A>>1)|((F&CF)<<7);F=(F&(SF|ZF|PF))|(A&(YF|XF))|c;}'
    ops[0x08] = '{uint32_t t=A;A=z->a_;z->a_=t;t=F;F=z->f_;z->f_=t;}'
    ops[0x10] = '{int8_t e=(int8_t)FETCH();B=(B-1)&255;if(B){PC=(PC+e)&0xffff;cyc+=5;}}'
    ops[0x18] = '{int8_t e=(int8_t)FETCH();PC=(PC+e)&0xffff;}'
    for i,c in enumerate(COND[:4]):
        ops[0x20+8*i] = f'{{int8_t e=(int8_t)FETCH();if({c}){{PC=(PC+e)&0xffff;cyc+=5;}}}}'
    ops[0x27] = 'OP_DAA();'
    ops[0x2F] = 'A^=255;F=(F&(SF|ZF|PF|CF))|HF|NF|(A&(YF|XF));'
    ops[0x37] = 'F=(F&(SF|ZF|PF))|CF|(A&(YF|XF));'
    ops[0x3F] = 'F=((F&(SF|ZF|PF|CF))|((F&CF)<<4)|(A&(YF|XF)))^CF;'
    ops[0x76] = 'HALT_OP();'
    for i,c in enumerate(COND):
        ops[0xC0+8*i] = f'if({c}){{PC=POP16();cyc+=6;}}'
        ops[0xC2+8*i] = f'{{uint32_t a=FETCH16();if({c})PC=a;}}'
        ops[0xC4+8*i] = f'{{uint32_t a=FETCH16();if({c}){{PUSH16(PC);PC=a;cyc+=7;}}}}'
        ops[0xC7+8*i] = f'PUSH16(PC);PC=0x{8*i:02X};'
    for i,p in enumerate(['BC','DE','HL','AF']):
        if p=='HL': continue
        if p=='AF':
            ops[0xF1] = '{uint32_t v=POP16();A=v>>8;F=v&255;}'
            ops[0xF5] = 'PUSH16((A<<8)|F);'
        else:
            ops[0xC1+16*i] = f'{{uint32_t v=POP16();{p[0]}=v>>8;{p[1]}=v&255;}}'
            ops[0xC5+16*i] = f'PUSH16(({p[0]}<<8)|{p[1]});'
    ops[0xC3] = 'PC=FETCH16();'
    ops[0xC9] = 'PC=POP16();'
    ops[0xCD] = '{uint32_t a=FETCH16();PUSH16(PC);PC=a;}'
    for k in range(8):
        ops[0xC6+8*k] = f'{{uint32_t v=FETCH();{alu(ALU[k],"v")}}}'
    ops[0xD3] = '{uint32_t p=FETCH();SYNC_OUT();PORT_OUT(p,A);SYNC_IN();}'
    ops[0xDB] = '{uint32_t p=FETCH();SYNC_OUT();A=PORT_IN(p);SYNC_IN();}'
    ops[0xD9] = '{uint32_t t;t=B;B=z->b_;z->b_=t;t=C;C=z->c_;z->c_=t;t=D;D=z->d_;z->d_=t;t=E;E=z->e_;z->e_=t;t=H;H=z->h_;z->h_=t;t=L;L=z->l_;z->l_=t;}'
    ops[0xEB] = '{uint32_t t=D;D=H;H=t;t=E;E=L;L=t;}'
    ops[0xF3] = 'z->iff1=z->iff2=0;'
    ops[0xFB] = 'z->iff1=z->iff2=1;irq_check=2;'
    return ops

def cb_ops():
    """CB xx on registers. (HL) handled with memory."""
    ops = {}
    ROT = {0:'RLC',1:'RRC',2:'RL',3:'RR',4:'SLA',5:'SRA',6:'SLL',7:'SRL'}
    for op in range(256):
        g, b, r = op>>6, (op>>3)&7, op&7
        if r == 6:
            src = 'v'; pre = 'uint32_t a=(H<<8)|L;uint32_t v=RD8(a);'; post = 'WR8(a,v);'
        else:
            src = R[r]; pre = f'uint32_t v={R[r]};'; post = f'{R[r]}=v;'
        if g == 0:
            ops[op] = '{'+pre+f'OP_{ROT[b]}(v);'+post+'}'
        elif g == 1:
            if r == 6: ops[op] = '{'+pre+f'OP_BITM(v,{b});'+'}'
            else: ops[op] = '{'+pre+f'OP_BIT(v,{b});'+'}'
        elif g == 2:
            ops[op] = '{'+pre+f'v&=~{1<<b}u;'+post+'}'
        else:
            ops[op] = '{'+pre+f'v|={1<<b}u;'+post+'}'
    return ops

def ddcb_ops():
    """DD/FD CB d xx : memory operand at a (precomputed), result optionally copied to register (undocumented)."""
    ops = {}
    ROT = {0:'RLC',1:'RRC',2:'RL',3:'RR',4:'SLA',5:'SRA',6:'SLL',7:'SRL'}
    for op in range(256):
        g, b, r = op>>6, (op>>3)&7, op&7
        copy = '' if r == 6 else f'{R[r]}=v;'
        if g == 0: ops[op] = f'{{uint32_t v=RD8(a);OP_{ROT[b]}(v);WR8(a,v);{copy}}}'
        elif g == 1: ops[op] = f'{{uint32_t v=RD8(a);OP_BITX(v,{b},a);}}'
        elif g == 2: ops[op] = f'{{uint32_t v=RD8(a)&~{1<<b}u;WR8(a,v);{copy}}}'
        else: ops[op] = f'{{uint32_t v=RD8(a)|{1<<b}u;WR8(a,v);{copy}}}'
    return ops

def ed_ops():
    ops = {}
    RR = ['BC','DE','HL','SP']
    def pg(p): return 'SP' if p=='SP' else f'(({p[0]}<<8)|{p[1]})'
    def ps(p,v): return f'SP=({v})&0xffff;' if p=='SP' else f'{{uint32_t _t=({v});{p[0]}=(_t>>8)&255;{p[1]}=_t&255;}}'
    for i,p in enumerate(RR):
        ops[0x42+16*i] = f'{{uint32_t v={pg(p)};OP_SBC16(v);}}'
        ops[0x4A+16*i] = f'{{uint32_t v={pg(p)};OP_ADC16(v);}}'
        ops[0x43+16*i] = f'{{uint32_t a=FETCH16();uint32_t v={pg(p)};WR8(a,v&255);WR8((a+1)&0xffff,v>>8);}}'
        ops[0x4B+16*i] = f'{{uint32_t a=FETCH16();uint32_t v=RD8(a)|(RD8((a+1)&0xffff)<<8);{ps(p,"v")}}}'
    for i in range(8):
        r = R[i]
        if i == 6:
            ops[0x40+8*i] = '{SYNC_OUT();uint32_t v=PORT_IN(C);SYNC_IN();F=(F&CF)|SZP[v];}'
            ops[0x41+8*i] = '{SYNC_OUT();PORT_OUT(C,0);SYNC_IN();}'
        else:
            ops[0x40+8*i] = f'{{SYNC_OUT();uint32_t v=PORT_IN(C);SYNC_IN();{r}=v;F=(F&CF)|SZP[v];}}'
            ops[0x41+8*i] = f'{{SYNC_OUT();PORT_OUT(C,{r});SYNC_IN();}}'
        ops[0x44+8*i] = '{uint32_t v=A;A=0;OP_SUB(v,0);}'
        ops[0x45+8*i] = 'PC=POP16();z->iff1=z->iff2;z->irqchk=1;'
        ops[0x46+8*i] = 'z->im=%d;' % [0,0,1,2,0,0,1,2][i]
    ops[0x4D] = 'PC=POP16();z->iff1=z->iff2;z->irqchk=1;'
    ops[0x47] = 'z->i=A;'
    ops[0x4F] = 'R=A;'
    ops[0x57] = 'A=z->i;F=(F&CF)|SZ[A]|(z->iff2?PF:0);'
    ops[0x5F] = 'A=(R&0x7f)|(z->rhi&0x80);F=(F&CF)|SZ[A]|(z->iff2?PF:0);'
    # R register write: keep high bit separately
    ops[0x4F] = 'R=A;z->rhi=A&0x80;'
    ops[0x67] = '{uint32_t a=(H<<8)|L;uint32_t m=RD8(a);WR8(a,((A<<4)|(m>>4))&255);A=(A&0xf0)|(m&15);F=(F&CF)|SZP[A];}'
    ops[0x6F] = '{uint32_t a=(H<<8)|L;uint32_t m=RD8(a);WR8(a,((m<<4)|(A&15))&255);A=(A&0xf0)|(m>>4);F=(F&CF)|SZP[A];}'
    ops[0xA0] = 'OP_LDI(1);'; ops[0xA8] = 'OP_LDI(-1);'
    ops[0xB0] = 'for(;;){OP_LDI(1);if(!(B|C))break;cyc+=5;if((int32_t)(cyc-target)>=0){PC=(PC-2)&0xffff;break;}cyc+=16;}'
    ops[0xB8] = 'for(;;){OP_LDI(-1);if(!(B|C))break;cyc+=5;if((int32_t)(cyc-target)>=0){PC=(PC-2)&0xffff;break;}cyc+=16;}'
    ops[0xA1] = 'OP_CPI(1);'; ops[0xA9] = 'OP_CPI(-1);'
    ops[0xB1] = 'OP_CPI(1);if((B|C)&&!(F&ZF)){PC=(PC-2)&0xffff;cyc+=5;}'
    ops[0xB9] = 'OP_CPI(-1);if((B|C)&&!(F&ZF)){PC=(PC-2)&0xffff;cyc+=5;}'
    ops[0xA2] = 'OP_INI(1);'; ops[0xAA] = 'OP_INI(-1);'
    ops[0xB2] = 'OP_INI(1);if(B){PC=(PC-2)&0xffff;cyc+=5;}'
    ops[0xBA] = 'OP_INI(-1);if(B){PC=(PC-2)&0xffff;cyc+=5;}'
    ops[0xA3] = 'OP_OUTI(1);'; ops[0xAB] = 'OP_OUTI(-1);'
    ops[0xB3] = 'for(;;){OP_OUTI(1);if(!B)break;cyc+=5;if((int32_t)(cyc-target)>=0){PC=(PC-2)&0xffff;break;}cyc+=16;}'
    ops[0xBB] = 'for(;;){OP_OUTI(-1);if(!B)break;cyc+=5;if((int32_t)(cyc-target)>=0){PC=(PC-2)&0xffff;break;}cyc+=16;}'
    return ops

def write_switch(name, ops, default):
    emit(f'/* {name} */')
    for op in range(256):
        if op in ops:
            emit(f'case 0x{op:02X}: {ops[op]} break;')
    emit(f'default: {default} break;')

m = main_ops()
assert len(m) == 256 - 4, sorted(set(range(256)) - set(m))   # CB DD ED FD handled by the core
emit('#define Z80F_MAIN_CASES \\'); 
lines = [f'case 0x{op:02X}: {m[op]} break; \\' for op in sorted(m)]
out += lines; emit('')
d = main_ops('XY')
emit('#define Z80F_XY_CASES \\')
out.extend(f'case 0x{op:02X}: {d[op].replace("EA_IX", "EA_XY")} break; \\' for op in sorted(d)); emit('')
e = ed_ops()
emit('#define Z80F_ED_CASES \\'); out.extend(f'case 0x{op:02X}: {e[op]} break; \\' for op in sorted(e)); emit('')
open('z80f_ops.inc','w').write('/* generated by gen.py - do not edit */\n' + '\n'.join(out) + '\n')
print('ok', len(out))
