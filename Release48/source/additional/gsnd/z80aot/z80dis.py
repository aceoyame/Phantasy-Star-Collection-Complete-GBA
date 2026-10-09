#!/usr/bin/env python3
"""minimal Z80 disassembler (official + common undocumented IX/IY forms).  z80dis.py FILE START END [BASE]"""
import sys
R=['B','C','D','E','H','L','(HL)','A']; RP=['BC','DE','HL','SP']; RP2=['BC','DE','HL','AF']; CC=['NZ','Z','NC','C','PO','PE','P','M']
ALU=['ADD A,','ADC A,','SUB ','SBC A,','AND ','XOR ','OR ','CP ']; ROT=['RLC','RRC','RL','RR','SLA','SRA','SLL','SRL']
def dis(m,pc):
    def b(i): return m[(pc+i)&0xFFFF]
    o=b(0); n=1; idx=None
    if o in (0xDD,0xFD):
        idx='IX' if o==0xDD else 'IY'; o=b(1); n=2
        if o in (0xDD,0xFD,0xED): return 1,'DB %02Xh'%b(0)
    def rn(r):
        if idx and r==6: return '(%s%+d)'%(idx,(b(n) ^128)-128)
        if idx and r==4: return idx+'H'
        if idx and r==5: return idx+'L'
        return R[r]
    def hl(): return idx or 'HL'
    x,y,z=o>>6,(o>>3)&7,o&7; p,q=y>>1,y&1
    dsp=1 if (idx and (z==6 or y==6) and x in(1,2)) else 0
    if o==0xCB:
        if idx:
            d=(b(2)^128)-128; o2=b(3); x,y,z=o2>>6,(o2>>3)&7,o2&7; t='(%s%+d)'%(idx,d)
            s=[ROT[y]+' '+t,'BIT %d,%s'%(y,t),'RES %d,%s'%(y,t),'SET %d,%s'%(y,t)][x]
            if x!=1 and z!=6: s+=','+R[z]
            return 4,s
        o2=b(1); x,y,z=o2>>6,(o2>>3)&7,o2&7
        return 2,[ROT[y]+' '+R[z],'BIT %d,%s'%(y,R[z]),'RES %d,%s'%(y,R[z]),'SET %d,%s'%(y,R[z])][x]
    if o==0xED and not idx:
        o=b(1); x,y,z=o>>6,(o>>3)&7,o&7; p,q=y>>1,y&1
        if x==1:
            if z==0: return 2,'IN %s,(C)'%('F' if y==6 else R[y])
            if z==1: return 2,'OUT (C),%s'%('0' if y==6 else R[y])
            if z==2: return 2,('SBC' if q==0 else 'ADC')+' HL,'+RP[p]
            if z==3:
                a=b(2)|b(3)<<8
                return 4,('LD (%04Xh),%s'%(a,RP[p])) if q==0 else ('LD %s,(%04Xh)'%(RP[p],a))
            if z==4: return 2,'NEG'
            if z==5: return 2,'RETN' if y!=1 else 'RETI'
            if z==6: return 2,'IM %s'%['0','0','1','2','0','0','1','2'][y]
            if z==7: return 2,['LD I,A','LD R,A','LD A,I','LD A,R','RRD','RLD','NOP','NOP'][y]
        if x==2 and z<4 and y>=4:
            return 2,['LDI','CPI','INI','OUTI','LDD','CPD','IND','OUTD','LDIR','CPIR','INIR','OTIR','LDDR','CPDR','INDR','OTDR'][(y-4)*4+z]
        return 2,'NOP*'
    if x==0:
        if z==0:
            if y==0: return n,'NOP'
            if y==1: return n,"EX AF,AF'"
            d=(b(n)^128)-128; t=(pc+n+1+d)&0xFFFF
            if y==2: return n+1,'DJNZ %04Xh'%t
            if y==3: return n+1,'JR %04Xh'%t
            return n+1,'JR %s,%04Xh'%(CC[y-4],t)
        if z==1:
            if q==0: return n+2,'LD %s,%04Xh'%(hl() if p==2 else RP[p],b(n)|b(n+1)<<8)
            return n,'ADD %s,%s'%(hl(),hl() if p==2 else RP[p])
        if z==2:
            if p==0: return n,'LD (BC),A' if q==0 else 'LD A,(BC)'
            if p==1: return n,'LD (DE),A' if q==0 else 'LD A,(DE)'
            a=b(n)|b(n+1)<<8
            if p==2: return n+2,('LD (%04Xh),%s'%(a,hl())) if q==0 else ('LD %s,(%04Xh)'%(hl(),a))
            return n+2,('LD (%04Xh),A'%a) if q==0 else ('LD A,(%04Xh)'%a)
        if z==3: return n,('INC %s' if q==0 else 'DEC %s')%(hl() if p==2 else RP[p])
        if z==4: return n+(1 if idx and y==6 else 0),'INC '+rn(y)
        if z==5: return n+(1 if idx and y==6 else 0),'DEC '+rn(y)
        if z==6:
            d=1 if idx and y==6 else 0
            return n+d+1,'LD %s,%02Xh'%(rn(y),b(n+d))
        return n,['RLCA','RRCA','RLA','RRA','DAA','CPL','SCF','CCF'][y]
    if x==1:
        if y==6 and z==6: return n,'HALT'
        d=1 if idx and (y==6 or z==6) else 0
        if idx and (y==6 or z==6):
            a=rn(y) if y!=6 else '(%s%+d)'%(idx,(b(n)^128)-128); c=rn(z) if z!=6 else '(%s%+d)'%(idx,(b(n)^128)-128)
            if y==6: a='(%s%+d)'%(idx,(b(n)^128)-128); c=R[z]
            if z==6: c='(%s%+d)'%(idx,(b(n)^128)-128); a=R[y]
            return n+1,'LD %s,%s'%(a,c)
        return n,'LD %s,%s'%(rn(y),rn(z))
    if x==2:
        d=1 if idx and z==6 else 0
        return n+d,ALU[y]+rn(z)
    if z==0: return n,'RET '+CC[y]
    if z==1:
        if q==0: return n,'POP '+(hl() if p==2 else RP2[p])
        return n,['RET','EXX','JP '+hl(),'LD SP,'+hl()][p] if p!=2 else 'JP (%s)'%hl()
    if z==2: return n+2,'JP %s,%04Xh'%(CC[y],b(n)|b(n+1)<<8)
    if z==3:
        if y==0: return n+2,'JP %04Xh'%(b(n)|b(n+1)<<8)
        if y==2: return n+1,'OUT (%02Xh),A'%b(n)
        if y==3: return n+1,'IN A,(%02Xh)'%b(n)
        if y==4: return n,'EX (SP),'+hl()
        if y==5: return n,'EX DE,HL'
        return n,'DI' if y==6 else 'EI'
    if z==4: return n+2,'CALL %s,%04Xh'%(CC[y],b(n)|b(n+1)<<8)
    if z==5:
        if q==0: return n,'PUSH '+(hl() if p==2 else RP2[p])
        return n+2,'CALL %04Xh'%(b(n)|b(n+1)<<8)
    if z==6: return n+1,ALU[y]+'%02Xh'%b(n)
    return n,'RST %02Xh'%(y*8)
if __name__=='__main__':
    m=open(sys.argv[1],'rb').read(); m=m+bytes(65536-len(m)); a=int(sys.argv[2],16); e=int(sys.argv[3],16)
    while a<e:
        n,s=dis(m,a); print('%04X  %-12s %s'%(a,' '.join('%02X'%m[(a+i)&0xFFFF] for i in range(n)),s)); a+=n
