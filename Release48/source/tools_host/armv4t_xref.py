"""Find Thumb/ARM PC-relative literal loads that reference given literal-pool addresses."""
import os
import struct,sys
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from armv4t_dis import Mem, dis
def thumb_xrefs(d, base, lits, lo, hi):
    out=[]
    for a in range(lo,hi,2):
        h=struct.unpack_from('<H',d,a-base)[0]
        if h>>11==9:
            t=((a+4)&~3)+(h&0xFF)*4
            if t in lits: out.append((a,t))
    return out
def arm_xrefs(d, base, lits, lo, hi):
    out=[]
    for a in range(lo,hi,4):
        w=struct.unpack_from('<I',d,a-base)[0]
        if (w&0x0F7F0000)==0x051F0000:  # ldr rd,[pc,#+-imm]
            imm=w&0xFFF; t=a+8+(imm if w>>23&1 else -imm)
            if t in lits: out.append((a,t))
    return out
