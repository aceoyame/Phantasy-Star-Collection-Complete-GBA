#!/usr/bin/env python3
"""Stream complete CP26/CP27 states at native text scan/event boundaries."""
from pathlib import Path
import subprocess,tempfile,json
R=Path(__file__).resolve().parents[2];A=R/'source/additional';V=R.parent/'validation_cp27'
with tempfile.TemporaryDirectory(prefix='cp27-scans-') as td:
 t=Path(td);h=t/'harness.c'
 h.write_text('#define main old_main\n#include "'+str(R/'source/tools_host/md_host/mdhost.c')+'"\n#undef main\n'+r'''
extern u32 aot_rgen[128];
void mdsnd_ym_68k(u32 a,u32 v){} void mdsnd_psg_68k(u32 v){}
void mdsnd_busreq(u32 v){} void mdsnd_reset(u32 v){}
static u8 orig[4096],page[4096];static u32 cases;
static void run(u32 pc,u32 a,u32 target,u32 budget,u32 variant){
 memcpy(page,orig,4096);m68k_romp[0x2b]=page;
 if(variant==1)page[0x856]^=1; /* modified indexed key */
 if(variant==2)page[0x856+5*4+2]=page[0x856+5*4+3]=0; /* early terminator */
 if(variant==3)page[0x856+92*4+3]=1; /* moved terminator */
 if(variant==4)page[0x856+2]=0xff,page[0x856+3]=0xfe; /* legal negative adjustment */
 if(variant==7)page[0x7b9]^=1;
 if(variant==8)page[0x7d8]=1;
 if(variant==9)page[0x7c5]=0;
 if(variant==10)page[0x7c5+12]=page[0x7c5+1];
 if(variant==5)page[0x856+5*4]=page[0x856],page[0x856+5*4+1]=page[0x857]; /* duplicate key changes */
 md_reset_game();md_cfg.index=variant==6?9:2;aot_select_index(2);memset(aot_rgen,0,512);
 for(u32 j=0;j<32768;j++)md_ram[j]=(u16)(j*271u+cases*83u);
 memset(&C,0,sizeof C);C.pc=pc;C.sr=(u16)(0x2700u|(cases&31u));C.cycles=(cases&1)?0xffffffe0u:0u;
 for(u32 j=0;j<8;j++){C.d[j]=0x87654300u+j;C.a[j]=0xffa000;}
 C.d[1]=0xdead0000u|target;C.a[0]=pc==0x2ba62?a:0x2b856;C.a[1]=pc==0x2ba0e?a:0x2b800;
 C.a[7]=0xff7004;C.d[0]=0x12340003;C.d[2]=0;C.d[3]=0;C.d[4]=3;C.d[5]=15;
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
 boot(argv[1]);memcpy(orig,m68k_romp[0x2b],4096);
 const u32 budgets[]={1,8,13,14,15,19,20,23,24,25,26,27,28,39,40,41,49,50,51,52,53,79,487,1024,2420};
 const u32 starts[]={0,1,5,45,91,92};
 for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)for(u32 k=0;k<96;k++)for(u32 i=0;i<6;i++){
  u32 target=k<92?((u32)orig[0x856+k*4]<<8)|orig[0x857+k*4]:k==92?0:k==93?0xffff:k==94?0x6161:0x6661;
  run(0x2ba62,0x2b856+4*starts[i]+(b&1),target,budgets[b],0);
 }
 const u32 addresses[]={0x2b856,0x2b857,0x2b852,0x2bffe,0x2bfff,0x3fffe,0x3ffff,0x40000,0xff9000,0xff9001,0xffffff,0xc00000,0x0122b856};
 for(u32 v=1;v<=6;v++)for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)for(u32 i=0;i<13;i++)run(0x2ba62,addresses[i],b&1?0x6661:0x6161,budgets[b],v);
 const u32 classaddr[]={0x2b802,0x2b806,0x2b800,0x2b856,0x2bffe,0x2bfff,0x3fffe,0x3ffff,0x40000,0xff9000,0xff9001,0xffffff,0xc00000,0x0122b802};
 for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)for(u32 i=0;i<14;i++)for(u32 k=0;k<16;k++)run(0x2ba0e,classaddr[i],(k*17)&255,budgets[b],0);
 for(u32 flags=0;flags<32;flags++)for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)run(flags&1?0x2ba62:0x2ba0e,flags&1?0x2b856:0x2b802,flags&2?0x6661:0x61,budgets[b],0);
 const u32 bases[]={0x2b7b9,0x2b7c5,0x2b7da,0x2b7dd},lens[]={10,19,1,3};
 const u32 realbudgets[]={25,51,52,53,79,487};
 for(u32 k=0;k<4;k++)for(u32 begin=0;begin<=lens[k];begin++)for(u32 target=0;target<32;target++)for(u32 b=0;b<6;b++){
  u32 letter=target<19?orig[0x7c5+target]:target<29?orig[0x7b9+target-19]:target==29?0:target==30?128:255;
  run(0x2ba0e,bases[k]+begin,letter,realbudgets[b],0);
 }
 for(u32 k=0;k<4;k++)for(u32 target=0;target<256;target++)for(u32 b=0;b<3;b++)run(0x2ba0e,bases[k],target,realbudgets[b+1],0);
 for(u32 v=7;v<=10;v++)for(u32 k=0;k<4;k++)for(u32 b=0;b<sizeof budgets/sizeof *budgets;b++)for(u32 d=0;d<2;d++)run(0x2ba0e,bases[k]+d,d?76:0x61,budgets[b],v);
 fprintf(stderr,"cases %u\n",cases);return fflush(stdout)!=0;
}
''')
 for tag,src in [('before',R.parent/'cp26_base'),('after',A)]:
  files=[h,src/'md.c',src/'m68k.c',A/'m68k_ref.c',A/'mdrender.c',src/'aot/aotdisp.c',src/'aot/generated/aot_code.c',src/'aot/generated/aot_tabs.c']
  p=subprocess.run(['gcc','-O2','-w','-DMD_HOST','-DAOT_ON','-ffunction-sections','-fdata-sections','-I'+str(src/'aot'),'-I'+str(A/'aot'),'-I'+str(A),*map(str,files),'-Wl,--gc-sections','-o',str(t/tag)],capture_output=True,text=True);assert p.returncode==0,p.stderr
 ps=[subprocess.Popen([str(t/tag),str(R/'additional_roms/Anne_Adventure_-_English.bin')],stdout=subprocess.PIPE,stderr=subprocess.PIPE) for tag in ['before','after']];pos=0
 try:
  while x:=ps[0].stdout.read(1<<20):
   y=ps[1].stdout.read(len(x))
   assert x==y,('state byte',pos+next(i for i,(a,b) in enumerate(zip(x,y)) if a!=b))
   pos+=len(x)
  assert not ps[1].stdout.read(1)
  assert all(p.wait()==0 for p in ps)
  cases=[int(p.stderr.read().split()[-1]) for p in ps];assert cases[0]==cases[1];assert pos%cases[0]==0
 finally:
  for p in ps:
   if p.poll() is None:p.kill()
 result={'all_pass':True,'cases':cases[0],'bytes_compared':pos,'baseline':'sealed CP26','coverage':'25 instruction/event budgets from 1 to 2420 cycles; all 92 real kerning keys and four absent/sentinel keys at six resume positions; odd/masked addresses; modified keys, early/moved terminators, changed negative adjustments, duplicate keys and Nei fallback; 16 class targets at 14 ROM/page/end/RAM/VDP addresses; four real class lists at every resume position/terminator with 32 targets, and all 256 targets at each class start; altered class characters, terminators and duplicate positions; all CCR bits; cycle wrap; exact CPU/bus/EEPROM/all memories/RGEN','excluded':'CPU optimization history and bus event context'}
 (V/'SCAN_BOUNDARIES_EXACT.json').write_text(json.dumps(result,indent=2)+'\n');print(result)
