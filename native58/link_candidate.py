#!/usr/bin/env python3
"""Incremental internal measurement link; does not package a release."""
from pathlib import Path
import os,sys,subprocess
W=Path(__file__).resolve().parents[1];S=W/'Release48/source';A=S/'additional';O=W/'native58/armcheck'
T=W/'Release48/compiler/root/usr';B=T/'lib/llvm-18/bin'
env=dict(os.environ,LD_LIBRARY_PATH=str(T/'lib/x86_64-linux-gnu'))
def run(*a):subprocess.run(list(map(str,a)),env=env,check=True)
for name in sys.argv[1:]:
 file=(A/'aot/generated'/name) if name in ('aot_code.c','aot_tabs.c') else (A/'aot'/name if name=='aotdisp.c' else A/name);obj=O/('md_'+file.name+'.o')
 flags=['--target=arm-none-eabi','-mcpu=arm7tdmi','-marm','-Os','-ffreestanding','-fno-builtin','-fno-unwind-tables','-I'+str(S/'psc8/include'),'-I'+str(A),'-I'+str(A/'aot'),'-DAOT_ON','-DAOT_ROM2']
 if file.name in ('aot_code.c','aot_tabs.c'):flags+=['-mthumb','-O2']
 if file.name=='mdsnd.c':flags+=['-O2']
 run(B/'clang',*flags,'-c',file,'-o',obj)
 if file.name in ('md.c','aotdisp.c','m68k.c','mdsndmix.c','mdrender.c'):run(B/'llvm-objcopy','--rename-section','.text=.hot,alloc,load,readonly,code,contents',obj)
 if file.name=='mdsnd.c':run(B/'llvm-objcopy','--rename-section','.text=.zaot,alloc,load,readonly,code,contents','--rename-section','.rodata=.zaot.ro,alloc,load,readonly,data,contents',obj)
objs=['md_start.s','mdmain.c','mdrt.c','m68k_ref.c','m68k.c','md.c','mdrender.c','mdsnd.c','mdsndmix.c','gsnd_bank.c','divmod.s','aotdisp.c','aot_code.c','aot_tabs.c']
run(B/'ld.lld','-T',A/'build/md.ld','-Map='+str(O/'md.map'),*[O/('md_'+n+'.o') for n in objs],'-o',O/'md.elf')
run(B/'llvm-objcopy','-O','binary',*[a for sec in ('rom2','sndidx','zaot','aotextra','aotmore') for a in ['--remove-section=.'+sec]],O/'md.elf',O/'md.bin')
rom=bytearray((W/'RUN_THIS_R48_CP57_NATIVE_WINDOW_LOOPS.gba').read_bytes())
for section,offset,limit in [('rom',0x6e2000,0x730000),('zaot',0x60000,0x78000),('aotmore',0x79000,0x7c000),('aotextra',0x6dbef8,0x6e2000),('rom2',0x1fe6750,0x2000000)]:
 p=O/('md.bin' if section=='rom' else 'md_'+section+'.bin')
 if section!='rom':run(B/'llvm-objcopy','-O','binary','--only-section=.'+section,O/'md.elf',p)
 data=p.read_bytes();assert offset+len(data)<=limit
 rom[offset:offset+len(data)]=data
(W/'native58/internal_candidate.gba').write_bytes(rom)
run(B/'llvm-nm','-S',O/'md.elf')
