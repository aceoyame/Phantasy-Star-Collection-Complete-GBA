from pathlib import Path
import sys,os,json,ctypes,subprocess,bisect,collections
W=Path(__file__).resolve().parents[1];D=W/'native58';R=W/'Release48'
sys.path.insert(0,str(R/'source/tools_host/cp09_prof'));os.environ['MD_ELF']=os.environ.get('MD_ELF',str(W/'cp57_base/md.elf'))
from gtest import boot,sym,L
L.mg_pprof_until.argtypes=[ctypes.c_uint32,ctypes.c_uint64];L.mg_pprof_until.restype=ctypes.c_uint64
idx=int(sys.argv[1]) if len(sys.argv)>1 else 2;start=int(sys.argv[2]) if len(sys.argv)>2 else 450;end=int(sys.argv[3]) if len(sys.argv)>3 else 750
rom=Path(os.environ.get('PROFILE_ROM',str(W/'RUN_THIS_R48_CP57_NATIVE_WINDOW_LOOPS.gba')));g=boot(rom,idx);entry=sym('md_begin_frame_input');machine=sym('M');zram=sym('md_zram');tick=sym('last_render_tick')
def inputs(f):
 keys=8 if 180<=f<184 else 2 if 400<=f<404 else 8 if 500<=f<504 else 1 if f>=600 and f%80<4 else 0
 g.wr16(machine+44,((keys&2)<<3)|((keys&1)<<5)|((keys&8)<<4))
 if f==150:g.wr8(zram+0x1c0a,0x81)
 if f in (149,449,749,1049):g.wr32(tick,0);g.wr32(machine+40,1)
for f in range(start+1):
 L.mg_break(entry);g.run(100);assert L.mg_hit()
 ret=g.reg(14)&~1;L.mg_break(ret);g.run(100);assert L.mg_hit();inputs(f)
g.save(D/f'game{idx}_frame{start}.ss');g.shot(D/f'game{idx}_frame{start}.png')
L.mg_break(0xffffffff);L.mg_pprof_init();c0=g.cycles()
frames=[]
for f in range(start,end):
 before=g.cycles();steps=L.mg_pprof_until(entry,4000000);assert steps<4000000,('frame cap',f)
 ret=g.reg(14)&~1;steps=L.mg_pprof_until(ret,4000000);assert steps<4000000,('input cap',f)
 inputs(f+1);frames.append({'frame':f,'cycles':g.cycles()-before})
 if f%50==49:print('profile frame',f+1,flush=True)
h=g.pprof_dump();syms=[]
for line in subprocess.run(['llvm-nm','-n','-S','--defined-only',os.environ['MD_ELF']],capture_output=True,text=True,check=True).stdout.splitlines():
 p=line.split()
 if len(p)==4 and p[2] in 'tTwW':syms.append((int(p[0],16)&~1,p[3],int(p[1],16)))
syms.sort();addrs=[a for a,_,_ in syms];by=collections.Counter();tot=0
for a,c in h:
 i=bisect.bisect_right(addrs,a)-1;name=syms[i][1] if i>=0 else '?';by[name]+=c;tot+=c
name=os.environ.get('PROFILE_PREFIX','WINDOW')+f'_{idx}_{start}_{end}'
(D/(name+'.json')).write_text(json.dumps({'index':idx,'guest_start':start,'guest_end':end,'cycles':g.cycles()-c0,'profiled_cycles':tot,'functions':[{'name':n,'cycles':c,'percent':100*c/tot} for n,c in by.most_common()],'frames':frames,'histogram':h},indent=2)+'\n')
print('total',tot,'cycles','ratio',(end-start)*280896/tot)
for n,c in by.most_common(35):print(n,round(100*c/tot,2),round(c/(end-start)))
