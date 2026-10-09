#!/usr/bin/env python3
"""Differential native-span tests across cycle boundaries, wrap and mapper writes."""
from pathlib import Path
import sys,os,subprocess,struct,tempfile,json,itertools,ctypes
R=Path(__file__).resolve().parents[2];V=R.parent/'validation_cp22'
if len(sys.argv)>1:
 tag,out=sys.argv[1:];base=tag=='before'
 elf=R/'source/references/cp21_final/psc8.elf' if base else R/'source/psc8/build/psc8.elf'
 syms={r[2]:int(r[0],16) for l in subprocess.check_output(['llvm-nm',str(elf)],text=True).splitlines() if len(r:=l.split())==3}
 sys.path.insert(0,str(R/'source/tools_host/cp08_mgba'));from mgba2 import GBA,L
 L.mg_break.argtypes=[ctypes.c_uint32];L.mg_set_reg.argtypes=[ctypes.c_int,ctypes.c_uint32]
 g=GBA(R/f'binaries/RUN_THIS_R48_{"CP21_ADVENTURE_NATIVE_DIV20" if base else "CP22_NATIVE_SPEED"}.gba',entry=0x086ca500)
 g.put(0x0203ff00,struct.pack('<IIII',166669648,262144,17,0))
 L.mg_break(syms['z80a_run']);g.run(100);assert L.mg_hit()
 ret=g.reg(14)&~1;ctx=syms['z80a_ctx']
 with tempfile.TemporaryDirectory() as td:
  state=Path(td)/'base';g.save(state)
  cases=[]
  for pc,b,h,budget in itertools.product([0x439,0x46c,0x4be,0x4cf],[1,8,0],[1,0x80,0xff],[7,8,23,24,25,69,70,71,73,74,75,102,103,104,300]):cases.append((pc,b,h,budget,0,0))
  for pc,dest,index in itertools.product([0x439,0x46c],[0xfffc,0xfffd,0xfffe,0xffff,0x8000],[0xffff,0x7fff,0xc100]):cases.append((pc,8,0x80,500,dest,index))
  cases += [(p,8,0x80,300,0,0) for p in [0x439,0x46c,0x4be,0x4cf]]
  with Path(out).open('wb') as f:
   for ci,(pc,b,h,budget,dest,index) in enumerate(cases):
    g.load(state)
    g.put(syms['ram'],bytes(((i*37+11)&255) for i in range(8192)))
    for off,val in [(0x120,index or 0xc100),(0x124,index or 0xc200),(0x164,0xa5),(0x168,0xff),(0x16c,(b<<8)|0x7f),(0x170,dest or 0xc300),(0x174,(h<<8)|0x55),(0x178,pc),(0x17c,0xc500)]:g.wr32(ctx+off,val)
    if ci>=len(cases)-4:g.wr32(ctx+0x1c4,0)
    L.mg_set_reg(0,budget);L.mg_break(ret);g.run(100);assert L.mg_hit(),(ci,pc,budget)
    raw=bytearray(struct.pack('<I',g.reg(0)))
    for off,n in [(0x120,8),(0x12c,12),(0x150,12),(0x164,28),(0x194,8),(0x1a8,20),(0x1bc,4)]:raw+=g.mem(ctx+off,n)
    for page in range(68):
     ptr=g.rd32(ctx+4*page)+page*1024
     raw+=g.mem(ptr,8)+g.mem(ptr+1016,8)
    for name,n in [('ram',8192),('sram',32768),('banks',4)]:raw+=g.mem(syms[name],n)
    f.write(raw)
 print(json.dumps({'cases':len(cases),'stride':len(raw)}));sys.exit(0)
with tempfile.TemporaryDirectory() as td:
 paths=[];stats=[]
 for tag in ['before','after']:
  p=Path(td)/tag;paths.append(p);r=subprocess.run([sys.executable,__file__,tag,str(p)],text=True,capture_output=True,check=True);stats.append(json.loads(r.stdout))
 a,b=[p.read_bytes() for p in paths]
 same=a==b
 if not same:
  i=next(i for i,(x,y) in enumerate(zip(a,b)) if x!=y);raise AssertionError(('case',i//stats[0]['stride'],'byte',i%stats[0]['stride'],a[i],b[i]))
 result={'all_pass':True,'cases':stats[0]['cases'],'bytes_compared':len(a),'coverage':'slice boundaries, repeated native loops, B=0, both RLC carries, both SRL triplets, index and destination wrap, SRAM/mapper writes, disabled specialization'}
 (V/'GAIDEN_SPANS_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
