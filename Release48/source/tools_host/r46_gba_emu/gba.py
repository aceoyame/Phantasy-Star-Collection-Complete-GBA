import ctypes, os
from PIL import Image
_here = os.path.dirname(os.path.abspath(__file__))
L = ctypes.CDLL(os.path.join(_here, 'libgba.so'))
L.gba_fb.restype = ctypes.POINTER(ctypes.c_uint32)
L.gba_mem.restype = ctypes.POINTER(ctypes.c_uint8)
for f in ('gba_rd8','gba_rd16','gba_rd32','gba_reg','gba_pcring'):
    getattr(L,f).restype = ctypes.c_uint32
L.gba_frame.restype = ctypes.c_uint64
L.gba_insns.restype = ctypes.c_uint64
KEYS = dict(A=1,B=2,SELECT=4,START=8,RIGHT=16,LEFT=32,UP=64,DOWN=128,R=256,L=512)

class GBA:
    def __init__(self, rom, entry=0):
        assert L.gba_load(rom.encode()) == 0
        L.gba_reset(ctypes.c_uint32(entry))
    def run(self, n=1, keys=0):
        if isinstance(keys,str): keys = self.k(keys)
        L.gba_set_keys(keys); L.gba_run_frames(n)
    @staticmethod
    def k(s):
        m=0
        for t in s.replace('+',' ').split():
            m|=KEYS[t.upper()]
        return m
    def press(self, keys, hold=3, after=10):
        self.run(hold, keys); self.run(after, 0)
    def shot(self, path=None, scale=1):
        fb = L.gba_fb()
        buf = bytes(ctypes.string_at(ctypes.cast(fb,ctypes.c_void_p), 240*160*4))
        im = Image.frombuffer('RGBX',(240,160),buf,'raw','BGRX',0,1).convert('RGB')
        if scale!=1: im = im.resize((240*scale,160*scale), Image.NEAREST)
        if path: im.save(path)
        return im
    def rd8(self,a): return L.gba_rd8(ctypes.c_uint32(a))
    def rd16(self,a): return L.gba_rd16(ctypes.c_uint32(a))
    def rd32(self,a): return L.gba_rd32(ctypes.c_uint32(a))
    def wr8(self,a,v): L.gba_wr8(ctypes.c_uint32(a),ctypes.c_uint32(v))
    def wr16(self,a,v): L.gba_wr16(ctypes.c_uint32(a),ctypes.c_uint32(v))
    def wr32(self,a,v): L.gba_wr32(ctypes.c_uint32(a),ctypes.c_uint32(v))
    def reg(self,i): return L.gba_reg(i)
    def mem(self, which, size):
        p = L.gba_mem(which); return bytes(ctypes.string_at(ctypes.cast(p,ctypes.c_void_p), size))
    def save(self,p): assert L.gba_save_state(p.encode())==0
    def load(self,p): assert L.gba_load_state(p.encode())==0
    def frame(self): return L.gba_frame()
    def pcs(self,n=16): return [hex(L.gba_pcring(i)) for i in range(n)]
def run_until_break(g,pc,maxframes=600):
    L.gba_set_break(ctypes.c_uint32(pc))
    for _ in range(maxframes):
        g.run(1, g._keys if hasattr(g,'_keys') else 0)
        if L.gba_break_hit(): L.gba_set_break(ctypes.c_uint32(0xFFFFFFFF)); return True
    L.gba_set_break(ctypes.c_uint32(0xFFFFFFFF)); return False
