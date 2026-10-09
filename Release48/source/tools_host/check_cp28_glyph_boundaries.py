#!/usr/bin/env python3
"""Stream exact CP27/CP28 glyph CPU and full bus-memory boundary states."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp28'
with tempfile.TemporaryDirectory(prefix='cp28-glyph-') as td:
 t=Path(td);h=t/'harness.c'
 h.write_text('#define main old_main\n#include "'+str(R/'source/tools_host/md_host/mdhost.c')+'"\n#undef main\n'+r'''
extern u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){} void mdsnd_psg_68k(u32 v){} void mdsnd_busreq(u32 v){} void mdsnd_reset(u32 v){}
static u32 cases;
static void run(u32 src,u32 dst,u32 count,u32 value,u32 c0,u32 c1,u32 budget){
 md_reset_game();aot_select_index(2);memset(aot_rgen,0,512);
 for(u32 j=0;j<32768;j++)md_ram[j]=(u16)(j*271u+cases*83u);
 if((src&0xffffffu)>=0xe00000u)for(u32 j=0;j<32;j++)((u8*)md_ram)[((src+j)&65535u)^1u]=(u8)(value+j*17u);
 memset(&C,0,sizeof C);C.pc=0x2baca;C.sr=(u16)(0x2700u|(cases&31u));C.cycles=(cases&1u)?0xffffffe0u:0u;
 for(u32 j=0;j<8;j++){C.d[j]=0x87654300u+j;C.a[j]=0xffa000;}
 C.d[0]=0x12340000u|count;C.d[4]=0xa5a50000u|c0;C.d[5]=0x5a5a0000u|c1;C.a[0]=dst;C.a[1]=src;C.a[2]=0xffe640;C.a[7]=0xff7004;
 m68k_exec(C.cycles+budget);
 M68K cpu=C;MD bus=M;
 cpu.spin_tgt=cpu.spin_fx=cpu.spin_cyc=cpu.spin_valid=cpu.spin_lrd=0;cpu.idle_ok=cpu.rcp_per=cpu.rcp=cpu.spin_wait=cpu.spin_back=0;
 memset(cpu.spin_regs,0,sizeof cpu.spin_regs);cpu.spin_skips=cpu.spin_cycles=0;memset(cpu.ring,0,sizeof cpu.ring);cpu.ring_pos=0;memset(bus.evctx,0,sizeof bus.evctx);
#define SAVE(x) if(fwrite(&(x),sizeof(x),1,stdout)!=1)exit(3)
 SAVE(cpu);SAVE(bus);SAVE(md_ee);SAVE(md_ram);SAVE(md_vram);SAVE(md_cram);SAVE(md_vsram);SAVE(md_zram);SAVE(aot_rgen);
#undef SAVE
 cases++;
}
int main(int argc,char**argv){
 boot(argv[1]);const u32 budgets[]={1,4,8,12,24,30,34,36,40,44,46,48,50,58,60,64,68,72,76,79,80,81,83,84,85,87,88,89,95,159,160,163,167,168,175,176,239,240,248,256,487,512,1024};
 const u32 edge[]={79,80,83,84,87,88,89,159,487};const u32 colors[][2]={{0,0},{0,15},{1,3},{15,0},{16,31},{128,255},{255,16},{0x85,0x42}};
 for(u32 v=0;v<256;v++)for(u32 c=0;c<8;c++)for(u32 b=0;b<9;b++)run(0xff9000,0xffa000,3,v,colors[c][0],colors[c][1],edge[b]);
 const u32 sources[]={0xff9000,0xff9001,0xfffffe,0xffffff,0x2b800,0x2bfff,0x3ffff,0xc00000};const u32 counts[]={0,1,7,0xffff};
 for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)for(u32 s=0;s<8;s++)for(u32 k=0;k<4;k++)for(u32 overlap=0;overlap<3;overlap++)for(u32 c=0;c<2;c++){
  u32 dst=overlap==0?0xffa000:overlap==1?sources[s]-1u:sources[s]+1u;
  run(sources[s],dst,counts[k],b*13u,colors[c+4][0],colors[c+4][1],budgets[b]);
 }
 const u32 dests[]={0xff00ff,0xff0100,0xff7fff,0xff8000,0xfffffe,0xffffff,0xc00000,0x01ffa000};
 for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)for(u32 d=0;d<8;d++)run(0x2b800,dests[d],7,b*3u,16,255,budgets[b]);
 fprintf(stderr,"cases %u\n",cases);return fflush(stdout)!=0;
}
''')
 for tag,src in [('before',R.parent/'cp27_base'),('after',A)]:
  files=[h,src/'md.c',src/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',src/'aot/aotdisp.c',src/'aot/generated/aot_code.c',src/'aot/generated/aot_tabs.c']
  p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(src/'aot'),'-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-Wl,--gc-sections','-o',str(t/tag)],capture_output=True,text=True);assert p.returncode==0,p.stderr
 ps=[subprocess.Popen([str(t/tag),str(R/'additional_roms/Anne_Adventure_-_English.bin')],stdout=subprocess.PIPE,stderr=subprocess.PIPE) for tag in ['before','after']];pos=0
 try:
  while x:=ps[0].stdout.read(1<<20):
   y=ps[1].stdout.read(len(x));assert x==y,('state byte',pos+next(i for i,(a,b) in enumerate(zip(x,y)) if a!=b));pos+=len(x)
  assert not ps[1].stdout.read(1) and all(p.wait()==0 for p in ps)
  cases=[int(p.stderr.read().split()[-1]) for p in ps];assert cases[0]==cases[1] and pos%cases[0]==0
 finally:
  for p in ps:
   if p.poll() is None:p.kill()
 result={'all_pass':True,'cases':cases[0],'bytes_compared':pos,'baseline':'sealed CP27','coverage':'43 budgets at instruction/80/84/88-cycle iteration boundaries; all 256 packed bytes; eight palette pairs including non-nibble values/carry/negative/zero; RAM/ROM/VDP sources, page/end fallback, counts 1/2/8/65536, ±1 overlap, odd/masked/wrapped RAM and code-generation boundaries, full CCR and cycle wrap; complete CPU/bus/EEPROM/all memories/RGEN','excluded':'CPU optimization history and bus event branch context'}
 (V/'GLYPH_BOUNDARIES_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
