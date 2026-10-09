"""Detect background-plane changes during a recorded action (nametable/tile/scroll)."""
import hashlib
def plane_sig(fr, which='B'):
    regs=fr['regs']; vram=fr['vram']
    base=((regs[4]&7)<<13) if which=='B' else ((regs[2]&0x38)<<10)
    w={0:32,1:64,3:128}.get(regs[16]&3,64); h={0:32,1:64,3:128}.get((regs[16]>>4)&3,32)
    nt=vram[base:base+w*h*2]
    tiles=set()
    for i in range(0,len(nt),2):
        tiles.add(((nt[i]<<8)|nt[i+1])&0x7FF)
    tb=b''.join(vram[t*32:t*32+32] for t in sorted(tiles))
    return hashlib.sha1(nt+tb).hexdigest()[:12]
def scan(rec, end=None):
    end=end or len(rec)
    b0=plane_sig(rec[0],'B'); a0=plane_sig(rec[0],'A')
    nb=sum(1 for t in range(end) if plane_sig(rec[t],'B')!=b0)
    na=sum(1 for t in range(end) if plane_sig(rec[t],'A')!=a0)
    ub=len(set(plane_sig(rec[t],'B') for t in range(end)))
    return nb,na,ub
