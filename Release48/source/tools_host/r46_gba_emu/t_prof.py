import sys,bisect,ctypes
from gtest import *
ROM='/home/claude/w/test_combo.gba'
L.gba_prof_hist.restype=ctypes.POINTER(ctypes.c_uint32); L.gba_prof_iw.restype=ctypes.POINTER(ctypes.c_uint32); L.gba_prof_other.restype=ctypes.c_uint32
syms=[]
for line in open(MAP):
    p=line.split()
    if len(p)>=5:
        try: syms.append((int(p[0],16),p[-1]))
        except: pass
syms.sort()
def nm(a):
    i=bisect.bisect_right(syms,(a,'~'))-1; return syms[i][1] if i>=0 else '?'
eid=int(sys.argv[1],16); sk=int(sys.argv[2],16)
g=gba_enemy_battle(ROM,'/home/claude/w/emu/tc_debug_menu.bin',eid)
defend_round(g); force_skill(g,sk)
run_until_break(g,sym('r46_clip_play_ex'),900); lr=g.reg(14)
L.gba_prof(2); f0=g.frame()
run_until_break(g,lr&~1,3000); L.gba_prof(0); print('frames',g.frame()-f0)
h=L.gba_prof_hist(); iw=L.gba_prof_iw()
cnt={}
import numpy as np
arr=np.ctypeslib.as_array(h,shape=(0x01800000>>4,)); nz=np.nonzero(arr)[0]
for i in nz: n=nm(0x08800000+i*16); cnt[n]=cnt.get(n,0)+int(arr[i])
arr2=np.ctypeslib.as_array(iw,shape=(0x8000>>4,)); 
for i in np.nonzero(arr2)[0]: n=nm(0x03000000+i*16); cnt['IW:'+n]=cnt.get('IW:'+n,0)+int(arr2[i])
cnt['other/bios']=L.gba_prof_other()
tot=sum(cnt.values())
for n,v in sorted(cnt.items(),key=lambda x:-x[1])[:22]: print('%5.1f%% %s'%(100*v/tot,n))
a0=sym('r46_clip_play_ex')
bins=sorted(((int(arr[i]),0x08800000+i*16) for i in nz if a0<=0x08800000+i*16<a0+0x1000),reverse=True)[:8]
print([(hex(a),v) for v,a in bins])
