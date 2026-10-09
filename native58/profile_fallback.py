from pathlib import Path
import subprocess,sys,json,re
W=Path(__file__).resolve().parents[1];D=W/'native58';A=W/'Release48/source/additional'
s=(A/'m68k.c').read_text().replace('    pc+=2u; cyc+=4u;','    {extern u32 count_pc[1<<24],count_op[65536],count_on;if(count_on){count_pc[pc&0xffffffu]++;count_op[op]++;}}\n    pc+=2u; cyc+=4u;')
(D/'profile_interpreter.c').write_text(s)
h='#define main old_main\n#include "'+str(W/'Release48/source/tools_host/md_host/mdhost.c')+'"\n#undef main\n'+r'''
void mdsnd_ym_68k(u32 a,u32 v){}void mdsnd_psg_68k(u32 v){}void mdsnd_busreq(u32 v){}void mdsnd_reset(u32 v){}
u32 count_pc[1<<24],count_op[65536],count_on;
int main(int argc,char**argv){boot(argv[1]);for(u32 f=0;f<450;f++){
 count_on=f>=150;
 u32 keys=f>=180&&f<184?K_START:f>=400&&f<404?K_B:0;
 md_begin_frame_input(keys);md_run(0);}
 for(u32 k=0;k<40;k++){u32 bi=0;for(u32 i=0;i<(1<<24);i++)if(count_pc[i]>count_pc[bi])bi=i;
 printf("PC %06x %u\n",bi,count_pc[bi]);count_pc[bi]=0;}
 for(u32 k=0;k<15;k++){u32 bi=0;for(u32 i=0;i<65536;i++)if(count_op[i]>count_op[bi])bi=i;
 printf("OP %04x %u\n",bi,count_op[bi]);count_op[bi]=0;}return 0;}
'''
(D/'profile_fallback.c').write_text(h)
files=[D/'profile_fallback.c',D/'profile_interpreter.c']+[A/n for n in ('md.c','m68k_ref.c','mdrender.c','aot/aotdisp.c','aot/generated/aot_code.c','aot/generated/aot_tabs.c')]
subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(A),'-I'+str(A/'aot'),*map(str,files),'-Wl,--gc-sections','-o',str(D/'profile_fallback')],check=True)
p=subprocess.run([str(D/'profile_fallback'),str(W/'Release48/additional_roms/Anne_Adventure_-_English.bin')],capture_output=True,text=True,check=True)
(D/'fallback_pc_counts.log').write_text(p.stdout)
sys.path.insert(0,str(A/'aot'));from aot68k import Tx
rom=(W/'Release48/additional_roms/Anne_Adventure_-_English.bin').read_bytes();tx=Tx(rom,0)
for line in p.stdout.splitlines()[:22]:
 _,pc,n=line.split();pc=int(pc,16)
 if pc<0x40000:ins=tx.insn(pc)
 elif 0xff0000<=pc<0xff8000:ins=tx.insn(pc-0xff0000+0x20000)
 else:continue
 print(line,'len',ins.length,'interp',ins.interp,'successors',','.join(hex(p) for p in ins.succ),'C',ins.c)
