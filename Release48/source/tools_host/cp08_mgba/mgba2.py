import ctypes,sys,os
from pathlib import Path
from PIL import Image
HERE=Path(__file__).resolve().parent
L=ctypes.CDLL(os.environ.get('CP08_PROBE_LIB',str(HERE/'libprobe2.so')))
for n in ['mg_rd8','mg_rd16','mg_rd32','mg_reg','mg_frame']:getattr(L,n).restype=ctypes.c_uint32
L.mg_cycles.restype=ctypes.c_uint64;L.mg_pixels.restype=ctypes.c_void_p;L.mg_pprof_get.restype=ctypes.c_uint64
L.mg_pprof_steps.argtypes=[ctypes.c_uint64]
KEYS=dict(A=1,B=2,SELECT=4,START=8,RIGHT=16,LEFT=32,UP=64,DOWN=128,R=256,L=512)
class GBA:
 def __init__(self,rom,entry=0x08000000):assert L.mg_load(str(rom).encode(),entry)==0
 def run(self,n=1,keys=0):L.mg_run(n,self.k(keys) if isinstance(keys,str) else keys)
 @staticmethod
 def k(s):return sum(KEYS[t.upper()] for t in s.replace('+',' ').split())
 def press(self,key,hold=3,after=10):self.run(hold,key);self.run(after)
 def rd8(self,a):return L.mg_rd8(a)
 def rd16(self,a):return L.mg_rd16(a)
 def rd32(self,a):return L.mg_rd32(a)
 def wr8(self,a,v):L.mg_wr8(a,v)
 def wr16(self,a,v):L.mg_wr16(a,v)
 def wr32(self,a,v):L.mg_wr32(a,v)
 def mem(self,a,n):
  buf=ctypes.create_string_buffer(n);L.mg_mem_read(a,buf,n);return buf.raw
 def put(self,a,data):L.mg_mem_write(a,ctypes.c_char_p(bytes(data)),len(data))
 def reg(self,i):return L.mg_reg(i)
 def frame(self):return L.mg_frame()
 def cycles(self):return L.mg_cycles()
 def save(self,p):assert L.mg_save(str(p).encode())
 def load(self,p):assert L.mg_restore(str(p).encode())
 def pcm(self,p=''):L.mg_pcm(str(p).encode())
 def shot(self,p=None,scale=1):
  im=Image.frombytes('RGBA',(240,160),ctypes.string_at(L.mg_pixels(),240*160*4)).convert('RGB')
  if scale!=1:im=im.resize((240*scale,160*scale),Image.Resampling.NEAREST)
  if p:im.save(p)
  return im
 def pprof(self,n):
  L.mg_pprof_init();L.mg_pprof_steps(n)
 def pprof_dump(self):
  import numpy as np
  buf=np.zeros(2*400000,dtype=np.uint64);n=L.mg_pprof_dump(buf.ctypes.data_as(ctypes.c_void_p),400000)
  return [(int(buf[2*i]),int(buf[2*i+1])) for i in range(n)]
 def pprof_dump_slow(self):
  out=[]
  for w,sz,base,shift in ((0,0x2000000//4,0x08000000,2),(1,0x8000//2,0x03000000,1),(2,0x40000//2,0x02000000,1)):
   for i in range(sz):
    c=L.mg_pprof_get(w,i)
    if c:out.append((base+(i<<shift),c))
  return out
