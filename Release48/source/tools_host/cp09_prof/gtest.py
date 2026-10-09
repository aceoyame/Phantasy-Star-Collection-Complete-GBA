import sys, os, subprocess, struct, wave, array, json
R=os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),'../../..'))
sys.path.insert(0, R+'/source/tools_host/cp08_mgba')
from mgba2 import GBA, L
import ctypes
L.mg_break.argtypes=[ctypes.c_uint32]
ELF=os.environ.get('MD_ELF',R+'/source/additional/build/md.elf')
def sym(name):
    for line in subprocess.run(['llvm-nm',ELF],capture_output=True,text=True,check=True).stdout.splitlines():
        p=line.split()
        if len(p)==3 and p[2]==name: return int(p[0],16)
    raise KeyError(name)
def boot(rom, idx):
    g=GBA(rom,entry=0x08000000); g.run(120)
    L.mg_break(0x080005f0); g.run(10,'START'); g.run(900); assert L.mg_hit(); L.mg_break(0xffffffff); g.run(50)
    for _ in range(4): g.press('DOWN',1,60)
    L.mg_break(0x08005000); g.run(10,'A'); g.run(500); assert L.mg_hit(); L.mg_break(0xffffffff); g.run(80)
    for _ in range(idx): g.press('DOWN',1,8)
    L.mg_break(0x086e2000); g.press('A',5,0); assert L.mg_hit(); L.mg_break(0xffffffff)
    return g
def wav(path, raw):
    d=array.array('h'); d.frombytes(raw)
    w=wave.open(path,'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(32768); w.writeframes(d.tobytes()); w.close()
def rms_blocks(raw, blk=32768):
    d=array.array('h'); d.frombytes(raw)
    out=[]
    for i in range(0,len(d)-2*blk+1,2*blk):
        s=d[i:i+2*blk]; out.append((sum(x*x for x in s)/len(s))**0.5)
    return out
