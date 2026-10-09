"""Actual ARM ABI/bit-exactness of the cold shift helper after relocation."""
from pathlib import Path
import os,sys,json,subprocess,ctypes
W=Path(__file__).resolve().parents[1];D=W/'native58';R=W/'Release48'
if len(sys.argv)>1:
 mode=sys.argv[1];os.environ['MD_ELF']=str(W/'cp57_base/md.elf' if mode=='ref' else D/'armcheck/md.elf')
 sys.path.insert(0,str(R/'source/tools_host/cp09_prof'));from gtest import boot,sym,L
 g=boot(W/'RUN_THIS_R48_CP57_NATIVE_WINDOW_LOOPS.gba' if mode=='ref' else D/'internal_candidate.gba',2);pc=sym('do_shift');rows=[]
 L.mg_set_reg.argtypes=[ctypes.c_int,ctypes.c_uint32];L.mg_cpsr.argtypes=[ctypes.c_uint32]
 L.mg_break(sym('md_begin_frame_input'));g.run(100);assert L.mg_hit()
 values=[0,1,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,0x7fffffff,0x80000000,0xffffffff,0x13579bdf]
 for mask in (255,65535,0xffffffff):
  for v in values:
   for n in (0,1,7,8,15,16,31,32,33,63):
    for kind,left in ((0,0),(1,0),(1,1),(3,0),(3,1)):
     for sr in (0x2700,0x271f):
      L.mg_break(0x08000100);L.mg_cpsr(0xdf)
      for i,x in enumerate((v,n,mask,kind)):L.mg_set_reg(i,x)
      sp=0x03007f00;g.wr32(sp,left);g.wr32(sp+4,sr);L.mg_set_reg(13,sp);L.mg_set_reg(14,0x08000100);L.mg_set_reg(15,pc)
      g.run(100);assert L.mg_hit(),(mask,v,n,kind,left,sr)
      assert g.reg(13)==sp
      rows.append([g.reg(0),g.reg(1)])
 (D/('SHIFT_'+mode+'.json')).write_text(json.dumps(rows));print(len(rows));sys.exit(0)
ps=[subprocess.Popen([sys.executable,__file__,m],stdout=subprocess.PIPE,stderr=subprocess.PIPE) for m in ('ref','native')]
for p in ps:
 out,err=p.communicate();assert p.returncode==0,err.decode()
a=json.loads((D/'SHIFT_ref.json').read_text());b=json.loads((D/'SHIFT_native.json').read_text());assert a==b
(D/'SHIFT_ARM_DIFFERENTIAL.json').write_text(json.dumps({'cases':len(a),'actual_ARM_result_full_CCR_and_stack_ABI_exact':True,'change':'do_shift relocated from IWRAM to ordinary ROM; code semantics retained'},indent=2)+'\n');print('PASS',len(a),'actual ARM shift calls')
