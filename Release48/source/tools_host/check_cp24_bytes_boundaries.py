#!/usr/bin/env python3
"""Direct differential graphics AOT verification around event and RAM boundaries."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp24'
with tempfile.TemporaryDirectory() as td:
 t=Path(td);h=t/'harness.c'
 h.write_text('''#define main old_main
#include "'''+str(R/'source/tools_host/md_host/mdhost.c')+'''"
#undef main
extern u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){} void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){} void mdsnd_reset(u32 v){}
int main(int argc,char**argv){
 boot(argv[1]);FILE*f=fopen(argv[2],"wb");u32 tests=0;
 const u32 budgets[]={1,11,12,15,16,17,47,48,49,63,64,65,127,128,129,255,256,257,1024};
 const u32 counts[]={0,1,2,3,4,7,31,0xffff};
 const u32 destinations[]={0xff9000,0xff9001,0xff7ffd,0xff7fff,0xff00fe,0xffffff,0xff3fff,0xc00000};
 for(u32 p=0;p<4;p++)for(u32 b=0;b<19;b++)for(u32 k=0;k<8;k++)for(u32 d=0;d<8;d++)for(u32 o=0;o<4;o++){
  md_reset_game();aot_select_index(2);memset(aot_rgen,0,512);
  for(u32 i=0;i<32768;i++)md_ram[i]=(u16)(i*271u+83u);
  memset(&C,0,sizeof C);C.pc=p==0?0x2bd28:p==1?0x2bd32:p==2?0x2b7ac:0x4224;C.sr=0x271f;
  C.d[0]=0x12340000|counts[k];C.d[2]=0;C.d[3]=0xabcd0000|(o*85u);
  C.d[1]=C.d[3];C.d[7]=C.d[3];
  C.a[0]=o==0?0xffa001:o==1?destinations[d]-1u:o==2?destinations[d]+1u:0xff7ffe;
  C.a[1]=destinations[d];if(p==2)C.a[0]=destinations[d];C.a[2]=0xffa000;C.a[7]=0xff7004;
  m68k_exec(budgets[b]);
  M68K cpu=C;cpu.spin_tgt=cpu.spin_fx=cpu.spin_cyc=cpu.spin_valid=cpu.spin_lrd=0;
  cpu.idle_ok=cpu.rcp_per=cpu.rcp=cpu.spin_wait=cpu.spin_back=0;
  memset(cpu.spin_regs,0,sizeof cpu.spin_regs);cpu.spin_skips=cpu.spin_cycles=0;
  memset(cpu.ring,0,sizeof cpu.ring);cpu.ring_pos=0;
  fwrite(&cpu,sizeof cpu,1,f);fwrite(md_ram,sizeof md_ram,1,f);fwrite(aot_rgen,512,1,f);tests++;
 }printf("%u\\n",tests);return fclose(f);
}
''')
 stats=[];paths=[]
 for tag,src in [('before',R/'source/references/cp21_final'),('after',A)]:
  files=[h,src/'md.c',src/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',src/'aot/aotdisp.c',src/'aot/generated/aot_code.c',src/'aot/generated/aot_tabs.c']
  c=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(src/'aot'),'-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-Wl,--gc-sections','-o',str(t/tag)],text=True,capture_output=True)
  if c.returncode:raise RuntimeError(c.stderr)
  out=t/(tag+'.state');paths.append(out);r=subprocess.run([str(t/tag),str(R/'additional_roms/Anne_Adventure_-_English.bin'),str(out)],capture_output=True,text=True,check=True);stats.append(int(r.stdout))
 stride=paths[0].stat().st_size//stats[0];pos=0
 with paths[0].open('rb') as a,paths[1].open('rb') as b:
  while x:=a.read(1<<20):
   y=b.read(len(x))
   if x!=y:
    i=pos+next(i for i,(u,v) in enumerate(zip(x,y)) if u!=v);raise AssertionError(('case',i//stride,'byte',i%stride,x[(i-pos)],y[(i-pos)]))
   pos+=len(x)
 result={'all_pass':True,'cases':stats[0],'bytes_compared':pos,'coverage':'copy/fill, 19 budgets, 8 counters, odd/even and page boundaries, RAM wrap and VDP fallback, forward overlap, values 00/55/AA/FF; CPU/RAM/RGEN exact'}
 (V/'RAM_BYTES_BOUNDARIES_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
