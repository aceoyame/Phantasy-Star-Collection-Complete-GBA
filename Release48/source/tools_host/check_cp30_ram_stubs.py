#!/usr/bin/env python3
"""Current-byte RAM jump shortcuts versus independent reference steps."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional'
with tempfile.TemporaryDirectory(prefix='cp30-stubs-') as td:
 t=Path(td);h=t/'h.c'
 h.write_text('#define main oldmain\n#include "'+str(R/'source/tools_host/md_host/mdhost.c')+'"\n#undef main\n#include "'+str(R.parent/'validation_cp30/REJECTED_ROM_RAM_STUBS.c')+'"\n'+r'''
void mdsnd_ym_68k(u32 a,u32 v){}void mdsnd_psg_68k(u32 v){}void mdsnd_busreq(u32 v){}void mdsnd_reset(u32 v){}
int main(int argc,char**argv){boot(argv[1]);u32 n=0;
const u32 pcs[]={0xe00000,0xff0092,0xfffff8,0xfffffa,0xfffffc,0x01ff0092,0xffff0092};
const u16 ops[]={0x6000,0x4ef8,0x4ef9};const u16 words[]={0,1,0x7fff,0x8000,0xffff,0x030c};
for(u32 p=0;p<7;p++)for(u32 op=0;op<3;op++)for(u32 w=0;w<6;w++)for(u32 sr=0;sr<32;sr++){
 u32 pc=pcs[p],o=(pc&65535u)>>1;if(ops[op]==0x4ef9&&o>0x7ffdu)continue;
 memset(&C,0,sizeof C);C.pc=pc;C.sr=0x2700|sr;C.cycles=(n&1)?0xfffffffcu:32;
 for(u32 k=0;k<16;k++)C.regs[k]=0x12345678u+k;
 md_ram[o]=ops[op];md_ram[o+1]=words[w];if(ops[op]==0x4ef9)md_ram[o+2]=0x1234;
 M68K initial=C;ref_step();M68K reference=C;C=initial;
 if(!ram_jump(pc&0xffffffu,0)||memcmp(&C,&initial,sizeof C))return 2;
 if(!ram_jump(pc&0xffffffu,1)||memcmp(&C,&reference,sizeof C))return 3;
 n++;
}
const u32 invalid[]={0x400,0xff0093,0xffffff};for(u32 p=0;p<3;p++)if(ram_jump(invalid[p],0))return 4;
md_ram[0x49]=0x6001;if(ram_jump(0xff0092,0))return 5;
printf("%u\n",n);return 0;}
''')
 files=[h,A/'md.c',A/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',A/'aot/generated/aot_code.c',A/'aot/generated/aot_tabs.c']
 p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-o',str(t/'test')],capture_output=True,text=True);assert p.returncode==0,p.stderr
 p=subprocess.run([str(t/'test'),str(R/'additional_roms/Anne_Adventure_-_English.bin')],capture_output=True,text=True,check=True)
 report={'all_pass':True,'production_enabled':False,'rejected_reason':'Actual GBA windows regressed with the ROM stub helper','cases':int(p.stdout),'oracle':'m68k_ref.c ref_step','opcodes':['BRA.W','JMP abs.w','JMP abs.l'],'cycles':[6,8,12],'coverage':'32 CCR states, signed targets, preserved PC high byte, cycle wrap, RAM aliases/endpoints, probe without mutation, opcode mutation and invalid-address declines'}
 (R.parent/'validation_cp30/RAM_STUBS_EXACT.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
