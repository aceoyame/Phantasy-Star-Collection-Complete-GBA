from pathlib import Path
import os,subprocess
P=Path(__file__).resolve().parent
O=P/'build';O.mkdir(exist_ok=True)
cc=os.environ.get('PSC8_CC','clang');link=os.environ.get('PSC8_LD','ld.lld');objcopy=os.environ.get('PSC8_OBJCOPY','llvm-objcopy')
flags=['-g','--target=arm-none-eabi','-mcpu=arm7tdmi','-O3','-ffreestanding','-fno-builtin','-fno-unwind-tables','-fno-exceptions','-I'+str(P/'include'),'-Wno-missing-braces','-Wno-incompatible-pointer-types-discards-qualifiers'] + os.environ.get('PSC8_CPPFLAGS','').split()
objects=[]
for file,arch in [('start.s','-marm'),('z80a.s','-marm'),('fm_steady.s','-marm'),('psc8.c','-marm'),('fast_opll.c','-marm'),('fast_opll_block.c','-marm'),('idle_probe.c','-mthumb')]:
 out=O/(file+'.o');subprocess.run([cc,*flags,arch]+(['-O2'] if file=='fast_opll_block.c' else ['-Os'] if file=='fast_opll.c' else ['-Oz'] if file=='idle_probe.c' else [])+['-c',str(P/file),'-o',str(out)],check=True)
 if file=='z80_gba.c':
  subprocess.run([objcopy,'--rename-section','.text=.hot,alloc,load,readonly,code,contents','--rename-section','.rodata=.hotdata,alloc,load,readonly,data,contents',str(out)],check=True)
 objects.append(str(out))
subprocess.run([link,'-T',str(P/'psc8.ld'),'-Map='+str(O/'psc8.map'),*objects,'-o',str(O/'psc8.elf')],check=True)
subprocess.run([objcopy,'-O','binary',str(O/'psc8.elf'),str(O/'psc8.bin')],check=True)
print('PSC8 module bytes', (O/'psc8.bin').stat().st_size)
