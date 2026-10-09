#!/usr/bin/env python3
"""Stream exact CP25/CP26 CPU and all bus-memory states at nibble-loop boundaries."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp26'
with tempfile.TemporaryDirectory(prefix='cp26-nibble-') as td:
 t=Path(td);h=t/'harness.c'
 h.write_text('''#define main old_main
#include "'''+str(R/'source/tools_host/md_host/mdhost.c')+'''"
#undef main
extern u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){} void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){} void mdsnd_reset(u32 v){}
int main(int argc,char**argv){
 boot(argv[1]);
 const u32 budgets[]={1,4,8,11,12,15,16,19,20,23,24,31,32,63,64,95,96,103,104,105,106,107,119,120,121,123,124,125,137,138,139,207,211,212,213,247,248,249,275,276,277,413,414,415,416,500,1024};
 const u32 counts[]={0,1,3,0xffff};
 const u32 dests[]={0xff9000,0xff9001,0xff00fe,0xff7fff,0xffffff,0xff3fff,0xc00000,0xffa000};
 const u32 transparency[]={0,1,15,16,255};
 for(u32 b=0;b<sizeof(budgets)/4;b++)for(u32 k=0;k<4;k++)for(u32 d=0;d<8;d++)for(u32 v=0;v<5;v++)for(u32 overlap=0;overlap<6;overlap++){
  md_reset_game();aot_select_index(2);memset(aot_rgen,0,512);
  for(u32 j=0;j<32768;j++)md_ram[j]=(u16)(j*271u+b*83u+v*0x1111u);
  memset(&C,0,sizeof C);C.pc=0x2bc12;C.sr=(u16)(0x2700u|((b+d+v)&31u));C.cycles=(v&1u)?0xffffffe0u:0u;
  for(u32 j=0;j<8;j++){C.d[j]=0x87654300u+j;C.a[j]=0xffa000;}
  C.d[0]=0x12340000|counts[k];C.d[1]=0;C.d[4]=3;C.d[5]=0xa5a50000|transparency[v];
  C.a[0]=0xff9000;C.a[1]=overlap==0?0xffa000:overlap==1?dests[d]-1u:overlap==2?dests[d]+1u:overlap==3?0x2b802:overlap==4?0x2bfff:0x3ffff;C.a[2]=dests[d];C.a[7]=0xff7004;
  m68k_exec(budgets[b]);
  M68K cpu=C;MD bus=M;
  cpu.spin_tgt=cpu.spin_fx=cpu.spin_cyc=cpu.spin_valid=cpu.spin_lrd=0;cpu.idle_ok=cpu.rcp_per=cpu.rcp=cpu.spin_wait=cpu.spin_back=0;
  memset(cpu.spin_regs,0,sizeof cpu.spin_regs);cpu.spin_skips=cpu.spin_cycles=0;memset(cpu.ring,0,sizeof cpu.ring);cpu.ring_pos=0;
  memset(bus.evctx,0,sizeof bus.evctx);
#define SAVE(x) if(fwrite(&(x),sizeof(x),1,stdout)!=1)return 3
  SAVE(cpu);SAVE(bus);SAVE(md_ee);SAVE(md_ram);SAVE(md_vram);SAVE(md_cram);SAVE(md_vsram);SAVE(md_zram);SAVE(aot_rgen);
#undef SAVE
 }return fflush(stdout)!=0;
}
''')
 for tag,src in [('before',R.parent/'cp25_base'),('after',A)]:
  files=[h,src/'md.c',src/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',src/'aot/aotdisp.c',src/'aot/generated/aot_code.c',src/'aot/generated/aot_tabs.c']
  p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(src/'aot'),'-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-Wl,--gc-sections','-o',str(t/tag)],capture_output=True,text=True)
  assert p.returncode==0,p.stderr
 ps=[subprocess.Popen([str(t/tag),str(R/'additional_roms/Anne_Adventure_-_English.bin')],stdout=subprocess.PIPE) for tag in ['before','after']]
 pos=0
 try:
  while x:=ps[0].stdout.read(1<<20):
   y=ps[1].stdout.read(len(x))
   assert x==y,('state byte',pos+next(i for i,(a,b) in enumerate(zip(x,y)) if a!=b))
   pos+=len(x)
  assert not ps[1].stdout.read(1)
  assert all(p.wait()==0 for p in ps)
 finally:
  for p in ps:
   if p.poll() is None:p.kill()
 cases=47*4*8*5*6;assert pos%cases==0
 result={'all_pass':True,'cases':cases,'bytes_compared':pos,'baseline':'sealed CP25','coverage':'47 event budgets; zero/one/multiple/65536 iterations; 5 transparency values; overlap ±1 and separate; ROM sources and page/end fallback; odd RAM/page/code-generation boundaries; RAM wrapping; VDP fallback; cycle wrap; exact CPU/bus/EEPROM/all memories/RGEN','excluded':'CPU optimization history and bus event context'}
 (V/'NIBBLE_BOUNDARIES_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
