import ctypes, os
from PIL import Image
L=ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)),'libmd.so'))
for f,t in [('md_fb',ctypes.POINTER(ctypes.c_uint32)),('md_ram',ctypes.POINTER(ctypes.c_uint8)),('md_vram',ctypes.POINTER(ctypes.c_uint8)),('md_cram',ctypes.POINTER(ctypes.c_uint16)),('md_vsram',ctypes.POINTER(ctypes.c_uint16)),('md_regs',ctypes.POINTER(ctypes.c_uint8)),('md_sram',ctypes.POINTER(ctypes.c_uint8)),('md_watch_log',ctypes.POINTER(ctypes.c_uint32))]:
    getattr(L,f).restype=t
L.md_reg.restype=ctypes.c_uint32; L.md_frame.restype=ctypes.c_uint64; L.md_cycles.restype=ctypes.c_uint64; L.md_hit_count.restype=ctypes.c_uint32
PAD=dict(UP=1,DOWN=2,LEFT=4,RIGHT=8,B=16,C=32,A=64,START=128)
ROM='/home/claude/w/R45/source/references/roms/ps4built.bin'
class MD:
    def __init__(self,rom=ROM):
        assert L.md_load(rom.encode())==0; L.md_reset()
    def run(self,n=1,keys=0):
        if isinstance(keys,str): keys=self.k(keys)
        L.md_set_pad(keys); L.md_run_frames(n)
    @staticmethod
    def k(s):
        m=0
        for t in s.replace('+',' ').split(): m|=PAD[t.upper()]
        return m
    def press(self,k,hold=3,after=10): self.run(hold,k); self.run(after,0)
    def shot(self,path=None,scale=1):
        b=ctypes.string_at(ctypes.cast(L.md_fb(),ctypes.c_void_p),320*224*4)
        im=Image.frombuffer('RGBX',(320,224),b,'raw','BGRX',0,1).convert('RGB')
        if scale!=1: im=im.resize((320*scale,224*scale),Image.NEAREST)
        if path: im.save(path)
        return im
    def ram(self): return bytearray(ctypes.string_at(ctypes.cast(L.md_ram(),ctypes.c_void_p),0x10000))
    def setram(self,addr,data):
        p=L.md_ram(); 
        for i,b in enumerate(data): p[(addr+i)&0xFFFF]=b
    def r16(self,a): r=L.md_ram(); a&=0xFFFF; return (r[a]<<8)|r[a+1]
    def r8(self,a): return L.md_ram()[a&0xFFFF]
    def w16(self,a,v): r=L.md_ram(); a&=0xFFFF; r[a]=(v>>8)&255; r[a+1]=v&255
    def w8(self,a,v): L.md_ram()[a&0xFFFF]=v&255
    def vram(self): return bytes(ctypes.string_at(ctypes.cast(L.md_vram(),ctypes.c_void_p),0x10000))
    def cram(self): return [L.md_cram()[i] for i in range(64)]
    def regs(self): return [L.md_regs()[i] for i in range(24)]
    def pc(self): return L.md_reg(16)
    def save(self,p): L.md_save(p.encode())
    def load(self,p): assert L.md_loadst(p.encode())==0
    def frame(self): return L.md_frame()
