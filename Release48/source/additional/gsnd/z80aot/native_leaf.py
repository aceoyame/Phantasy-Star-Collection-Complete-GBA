"""Inline a verified leaf while keeping exact instruction/stack timing."""
import z80aot as z
class Leaf(z.Gen):
 def emit(self,i):
  if i.kind!='ret':return super().emit(i)
  start=self.pt;self.w('  Z80T(0x%04x,%d,%d);'%(i.addr,self.pt,self.pr));self.pt+=i.cyc;self.pr+=i.m1;self.flush()
  self.w('#undef TS\n#undef TE\n#define TS (cyc-%d)\n#define TE cyc'%(i.cyc))
  ret='PC=POP16();if(PC!=0x%04x)goto dispatch;goto leaf_done;'%self.return_pc
  if i.cond:self.w('  if(%s){cyc+=6;%s}'%(i.cond,ret))
  else:self.w('  '+ret);self.dead=True
def pitch(mem,return_pc,call_pc):
 ins,leaders=z.discover(mem,[0x434],[]);leaders|={0x434}
 assert all(0x434<=pc<0x47a and i.kind in ('seq','jp','jr','ret') for pc,i in ins.items())
 budget=sum(i.cyc+(5 if i.kind=='jr' and i.cond else 6 if i.kind=='ret' and i.cond else 0) for i in ins.values())
 l=Leaf(mem,ins,leaders);l.mem_w=bytes(mem);l.dead=True;l.return_pc=return_pc
 for pc,i in sorted(ins.items()):
  if pc in leaders:
   if not l.dead:l.flush()
   l.pt=l.pr=0;l.w('L_%04x:'%pc);l.dead=False
  l.emit(i)
 assert l.dead
 l.w('leaf_done:;')
 import re
 text='\n'.join(l.out);text=re.sub(r'L_([0-9a-f]{4})',lambda m:'P_%04x_%s'%(call_pc,m[1]),text).replace('leaf_done','P_%04x_done'%call_pc)
 return text,budget
