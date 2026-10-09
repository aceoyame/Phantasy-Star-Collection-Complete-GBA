import struct
def dump_oam(g, only_visible=True):
    oam=g.mem(5,0x400); out=[]
    for i in range(128):
        a0,a1,a2=struct.unpack_from('<3H',oam,i*8)
        if only_visible and ((a0&0x300)==0x200 or (a0&0xff) in (160,)): continue
        out.append((i,hex(a0),hex(a1),hex(a2), 'y',a0&0xff,'x',a1&0x1ff,'tile',a2&0x3ff,'pal',a2>>12,'prio',(a2>>10)&3))
    return out
def objpal(g,bank):
    p=g.mem(3,0x400); return ['%04x'%x for x in struct.unpack_from('<16H',p,0x200+bank*32)]
