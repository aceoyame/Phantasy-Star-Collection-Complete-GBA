#!/usr/bin/env python3
"""Stream exact CP27/CP30 glyph CPU and full bus-memory boundary states."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp30'
with tempfile.TemporaryDirectory(prefix='cp30-glyph-') as td:
 t=Path(td);h=t/'harness.c'
 h.write_text('#define main old_main\n#include "'+str(R/'source/tools_host/md_host/mdhost.c')+'"\n#undef main\n'+r'''
extern u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){} void mdsnd_psg_68k(u32 v){} void mdsnd_busreq(u32 v){} void mdsnd_reset(u32 v){}
static u32 cases;


static void run(u32 src,u32 dst,u32 budget,u32 sp){
 md_reset_game();aot_select_index(9);memset(aot_rgen,0,512);
 for(u32 j=0;j<32768;j++){md_ram[j]=(u16)(j*271u+cases*83u);md_vram[j]=(u16)(j*17u);}
 memset(&C,0,sizeof C);C.pc=0x3b9c;C.sr=(u16)(0x2700u|(cases&31u));C.cycles=(cases&1u)?0xffffffe0u:0u;
 for(u32 j=0;j<8;j++){C.d[j]=0x87654300u+j;C.a[j]=0xffa000;}
 C.d[0]=cases*0x43217u;C.a[2]=src;C.a[4]=dst;C.a[7]=sp;
 md_ram[0x3802]=0;md_ram[0x3803]=0x400;
 if((src&0xffffffu)>=0xe00000u){u32 o=src&65535u;for(u32 j=0;j<4;j++)((u8*)md_ram)[((o+j)&65535u)^1u]=(u8)(cases*19u+j*73u);}
 M.code=(cases%3u==0)?3u:(cases%3u==1)?1u:5u;M.addr=(cases&1u)?0xfffeu:0x9000u;M.reg[15]=(cases&2u)?2u:128u;
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
 boot(argv[1]);const u32 budgets[]={1,7,8,9,19,20,21,27,28,29,67,68,69,75,76,77,87,88,89};
 const u32 sources[]={0xff9000,0xff9001,0xfffffc,0xfffffd,0xffffff,0xff7ffe,0xe00000,0x3b98,0x03fffd,0x040000,0xc00000,0x01ff9000};
 const u32 dests[]={0xffa000,0xc00000,0xc00002,0xc00004,0xff00ff,0xff7fff,0xff7004,0xff7006};
 for(u32 b=0;b<19;b++)for(u32 src=0;src<12;src++)for(u32 d=0;d<8;d++)run(sources[src],dests[d],budgets[b],0xff7004);
 for(u32 val=0;val<256;val++)run(0xff9000,0xffa000,88u,0xff7004);
 const u32 stacks[]={0xff7005,0xfffffa,0xfffffc,0xffffff,0x3ba0,0xc00000,0x01ff7004};
 for(u32 b=0;b<19;b++)for(u32 sp=0;sp<7;sp++)run(0xff7004,0xff7004,budgets[b],stacks[sp]);
 fprintf(stderr,"cases %u\n",cases);return fflush(stdout)!=0;
}
''')
 for tag,src in [('before',R.parent/'cp29_base'),('after',A)]:
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
 result={'all_pass':True,'cases':cases[0],'bytes_compared':pos,'baseline':'sealed CP29','coverage':'19 event budgets; 256 input-byte patterns, RAM/ROM/MMIO sources, odd/masked/wrapped endpoints, VDP/RAM/code stores, full CCR/cycle wrap, VRAM/CRAM/VSRAM, real stack return and source/destination/stack overlap; complete CPU/bus/EEPROM/all memories/RGEN','excluded':'CPU optimization history and bus event branch context'}
 (V/'TILE_PACK_BOUNDARIES_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
